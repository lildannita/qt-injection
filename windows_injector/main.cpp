#ifdef _WIN32

#include <windows.h>

#include <iostream>
#include <string>
#include <vector>

struct ProcessHandles {
    HANDLE hProcess = nullptr;
    HANDLE hThread  = nullptr;

    ~ProcessHandles()
    {
        if (hThread)
            CloseHandle(hThread);
        if (hProcess)
            CloseHandle(hProcess);
    }

    ProcessHandles() = default;
    ProcessHandles(const ProcessHandles &) = delete;
    ProcessHandles &operator=(const ProcessHandles &) = delete;
};

struct RemoteMemory {
    HANDLE hProcess = nullptr;
    LPVOID address  = nullptr;

    ~RemoteMemory()
    {
        if (address && hProcess)
            VirtualFreeEx(hProcess, address, 0, MEM_RELEASE);
    }

    RemoteMemory() = default;
    RemoteMemory(const RemoteMemory &) = delete;
    RemoteMemory &operator=(const RemoteMemory &) = delete;
};

static void logError(const wchar_t *msg) noexcept
{
    const auto err = GetLastError();
    std::wcerr << L"[injector] " << msg << L"  (error " << err << L")" << std::endl;
}

static bool inject(const std::wstring &targetExe,
                   const std::wstring &dllPath,
                   const std::wstring &extraArgs)
{
    // ── Step 1: Build the command line ───────────────────────────────────
    //
    // [EN] CreateProcessW requires a MUTABLE (non-const) wide-char buffer
    //      for the command line - this is a documented requirement, the
    //      function may temporarily modify the string.  We use std::vector
    //      to provide a writable buffer.  Quotes around the exe path handle
    //      spaces (e.g. "C:\Program Files\...").
    //
    // [RU] CreateProcessW требует МУТАБЕЛЬНЫЙ (non-const) буфер wide-символов
    //      для командной строки - это документированное требование, функция
    //      может временно модифицировать строку.  Используем std::vector
    //      для предоставления записываемого буфера.  Кавычки вокруг пути
    //      к exe нужны для обработки пробелов (напр. "C:\Program Files\...").
    auto cmdLine = L"\"" + targetExe + L"\"";
    if (!extraArgs.empty()) {
        cmdLine += L" " + extraArgs;
    }
    auto cmdBuf = std::vector<wchar_t>(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    // ── Step 2: Create the target process suspended ─────────────────────
    //
    // [EN] CREATE_SUSPENDED: the main thread is created but never scheduled.
    //      No user code, no CRT init, no DllMain of any implicitly linked
    //      DLL has run yet.  This is our window to inject the agent.
    //
    // [RU] CREATE_SUSPENDED: главный поток создан, но не запланирован к
    //      исполнению.  Ни пользовательский код, ни инициализация CRT,
    //      ни DllMain неявно подключённых DLL ещё не выполнились.  Это
    //      наше окно для внедрения агента.
    auto si = STARTUPINFOW{};
    si.cb = sizeof(si);
    auto pi = PROCESS_INFORMATION{};
    if (!CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED,
                        nullptr, nullptr, &si, &pi)) {
        logError(L"CreateProcessW failed");
        return false;
    }

    ProcessHandles handles;
    handles.hProcess = pi.hProcess;
    handles.hThread  = pi.hThread;

    std::wcout << L"[injector] PID " << pi.dwProcessId
               << L"  created suspended." << std::endl;

    // ── Step 3: Allocate memory in the target process ───────────────────
    //
    // [EN] We need to place the DLL path string where the target process
    //      can read it.  Our own memory is inaccessible to the target -
    //      each process has its own virtual address space.  VirtualAllocEx
    //      allocates pages in the target's address space.
    //
    //      MEM_COMMIT | MEM_RESERVE: reserve a range of virtual addresses
    //      AND back them with physical storage (page file) in one call.
    //      PAGE_READWRITE: the pages are readable and writable (we need
    //      to write the path, LoadLibraryW needs to read it).
    //
    // [RU] Нам нужно поместить строку пути к DLL туда, где целевой процесс
    //      сможет её прочитать.  Наша память недоступна целевому процессу -
    //      у каждого процесса своё виртуальное адресное пространство.
    //      VirtualAllocEx выделяет страницы в адресном пространстве цели.
    //
    //      MEM_COMMIT | MEM_RESERVE: резервируем диапазон виртуальных
    //      адресов И подкрепляем их физической памятью (файл подкачки)
    //      за один вызов.
    //      PAGE_READWRITE: страницы доступны для чтения и записи (нам
    //      нужно записать путь, а LoadLibraryW - прочитать его).
    const auto pathBytes = (dllPath.size() + 1) * sizeof(wchar_t);
    RemoteMemory remoteMem;
    remoteMem.hProcess = pi.hProcess;
    remoteMem.address = VirtualAllocEx(
        pi.hProcess, nullptr, pathBytes,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!remoteMem.address) {
        logError(L"VirtualAllocEx failed");
        TerminateProcess(pi.hProcess, 1);
        return false;
    }

    if (!WriteProcessMemory(pi.hProcess, remoteMem.address,
                            dllPath.c_str(), pathBytes, nullptr)) {
        logError(L"WriteProcessMemory failed");
        TerminateProcess(pi.hProcess, 1);
        return false;
    }

    // ── Step 5: Get the address of LoadLibraryW ─────────────────────────
    //
    // [EN] LoadLibraryW is exported by kernel32.dll, which is loaded into
    //      every Windows process.  Due to how Windows ASLR works for system
    //      DLLs, kernel32.dll is mapped at the SAME virtual address in all
    //      processes within a single boot session.  This means the pointer
    //      we get from GetProcAddress in OUR process is equally valid in
    //      the TARGET process.
    //
    //      This is the fundamental trick that makes the CreateRemoteThread
    //      injection technique possible.  If kernel32 were at different
    //      addresses in different processes, we'd need a different approach
    //      (e.g. manual mapping, shellcode, or APC injection).
    //
    // [RU] LoadLibraryW экспортируется из kernel32.dll, которая загружена
    //      в каждый процесс Windows.  Благодаря тому, как работает ASLR
    //      для системных DLL, kernel32.dll отображается по ОДНОМУ И ТОМУ ЖЕ
    //      виртуальному адресу во всех процессах в пределах одной загрузки ОС.
    //      Это значит, что указатель, полученный через GetProcAddress в НАШЕМ
    //      процессе, равно валиден в ЦЕЛЕВОМ.
    //
    //      Это фундаментальный трюк, делающий технику инъекции через
    //      CreateRemoteThread возможной.  Если бы kernel32 располагался
    //      по разным адресам в разных процессах, пришлось бы использовать
    //      другой подход (manual mapping, shellcode или APC-инъекция).
    const auto hKernel = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel) {
        logError(L"GetModuleHandleW(kernel32) failed");
        TerminateProcess(pi.hProcess, 1);
        return false;
    }

    const auto fnLoadLib = GetProcAddress(hKernel, "LoadLibraryW");
    if (!fnLoadLib) {
        logError(L"GetProcAddress(LoadLibraryW) failed");
        TerminateProcess(pi.hProcess, 1);
        return false;
    }

    // ── Step 6: Create a remote thread calling LoadLibraryW ─────────────
    // [EN] CreateRemoteThread creates a new thread in the target process.
    //      We pass:
    //        - pStartRoutine = address of LoadLibraryW  (the thread function)
    //        - remoteMem.address = the DLL path string  (the argument)
    //
    //      The thread will execute:  LoadLibraryW(L"C:\\path\\to\\agent.dll")
    //      inside the target process.  This causes the OS loader to:
    //        a) Map agent.dll into the target's address space
    //        b) Resolve its imports (Qt5Core.dll, etc.)
    //        c) Call DllMain(DLL_PROCESS_ATTACH)
    //        d) Execute static initializers in agent.dll
    //
    //      The cast from FARPROC to LPTHREAD_START_ROUTINE is technically
    //      a type mismatch (FARPROC is int(*)() while LPTHREAD_START_ROUTINE
    //      is DWORD(WINAPI*)(LPVOID)), but it is safe in practice because
    //      both are __stdcall with one pointer-sized parameter and a
    //      pointer-sized return value.  GCC/Clang warn about this cast;
    //      the pragma suppresses that specific warning.
    //
    // [RU] CreateRemoteThread создаёт новый поток в целевом процессе.
    //      Мы передаём:
    //        - pStartRoutine = адрес LoadLibraryW  (функция потока)
    //        - remoteMem.address = строка пути к DLL  (аргумент)
    //
    //      Поток выполнит:  LoadLibraryW(L"C:\\path\\to\\agent.dll")
    //      внутри целевого процесса.  Это заставит загрузчик ОС:
    //        a) Отобразить agent.dll в адресное пространство цели
    //        b) Разрешить его импорты (Qt5Core.dll и т.д.)
    //        c) Вызвать DllMain(DLL_PROCESS_ATTACH)
    //        d) Выполнить статические инициализаторы в agent.dll
    //
    //      Каст из FARPROC в LPTHREAD_START_ROUTINE формально является
    //      несовпадением типов (FARPROC - int(*)(), LPTHREAD_START_ROUTINE -
    //      DWORD(WINAPI*)(LPVOID)), но безопасен на практике, поскольку обе
    //      функции - __stdcall с одним аргументом размером с указатель и
    //      возвратом размером с указатель.  GCC/Clang предупреждают об
    //      этом касте; pragma подавляет конкретно это предупреждение.
#if defined(__GNUC__) || defined(__clang__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
    const auto pStartRoutine = reinterpret_cast<LPTHREAD_START_ROUTINE>(fnLoadLib);
#if defined(__GNUC__) || defined(__clang__)
#   pragma GCC diagnostic pop
#endif

    const auto hRemote = CreateRemoteThread(
        pi.hProcess, nullptr, 0,
        pStartRoutine, remoteMem.address, 0, nullptr);

    if (!hRemote) {
        logError(L"CreateRemoteThread failed");
        TerminateProcess(pi.hProcess, 1);
        return false;
    }

    // [EN] Block until LoadLibraryW returns in the target process.
    //      Typically completes in milliseconds.
    // [RU] Блокируемся до завершения LoadLibraryW в целевом процессе.
    //      Обычно завершается за миллисекунды.
    WaitForSingleObject(hRemote, INFINITE);

    // [EN] Check the result of LoadLibraryW in the remote process.
    //      exitCode == 0:            LoadLibraryW returned NULL - generic failure.
    //      exitCode >= 0xC0000000:   NTSTATUS error code (e.g. 0xC0000135 =
    //                                STATUS_DLL_NOT_FOUND - the agent DLL or one
    //                                of its dependencies was not found).
    //      Otherwise:                HMODULE base address - success.
    //
    // [RU] Проверяем результат LoadLibraryW в удалённом процессе.
    //      exitCode == 0:            LoadLibraryW вернул NULL - общая ошибка.
    //      exitCode >= 0xC0000000:   NTSTATUS код ошибки (напр. 0xC0000135 =
    //                                STATUS_DLL_NOT_FOUND - DLL агента или одна
    //                                из его зависимостей не найдена).
    //      Иначе:                    базовый адрес HMODULE - успех.
    auto exitCode = DWORD{ 0 };
    GetExitCodeThread(hRemote, &exitCode);
    CloseHandle(hRemote);

    if (exitCode == 0) {
        std::wcerr << L"[injector] ERROR: LoadLibraryW returned NULL - "
                      L"DLL failed to load. Check the path." << std::endl;
        TerminateProcess(pi.hProcess, 1);
        return false;
    }
    if (exitCode >= 0xC0000000) {
        std::wcerr << L"[injector] ERROR: LoadLibraryW failed with NTSTATUS 0x"
                   << std::hex << exitCode << std::dec << std::endl;
        if (exitCode == 0xC0000135) {
            std::wcerr << L"  STATUS_DLL_NOT_FOUND - the agent DLL or its "
                          L"dependencies (Qt5Core.dll, etc.) were not found.\n"
                          L"  Ensure Qt bin directory is in PATH." << std::endl;
        }
        TerminateProcess(pi.hProcess, 1);
        return false;
    }
    std::wcout << L"[injector] DLL loaded at remote base 0x"
               << std::hex << exitCode << std::dec << std::endl;

    // ── Step 7: Resume the main thread ──────────────────────────────────
    // [EN] The target process has been suspended since creation.  Our agent
    //      DLL is now loaded and its Q_COREAPP_STARTUP_FUNCTION is registered.
    //      ResumeThread decrements the thread's suspend count; when it
    //      reaches zero, the thread starts executing.  From this point on,
    //      the application runs normally: CRT init → static constructors →
    //      main() → QCoreApplication ctor → our hooks fire.
    //
    // [RU] Целевой процесс приостановлен с момента создания.  Наша DLL-агент
    //      загружена, Q_COREAPP_STARTUP_FUNCTION зарегистрирован.
    //      ResumeThread уменьшает счётчик приостановки потока; когда он
    //      достигает нуля, поток начинает выполняться.  С этого момента
    //      приложение работает нормально: инициализация CRT → статические
    //      конструкторы → main() → конструктор QCoreApplication → наши
    //      хуки срабатывают.
    if (ResumeThread(pi.hThread) == static_cast<DWORD>(-1)) {
        logError(L"ResumeThread failed");
        TerminateProcess(pi.hProcess, 1);
        return false;
    }

    std::wcout << L"[injector] Main thread resumed. Injection complete." << std::endl;
    std::wcout << L"[injector] Waiting for target process to exit..."
               << std::endl;

    // [EN] Wait for the target process to finish.  This keeps the injector
    //      (and its parent — the Python script) alive, so the console stays
    //      open and stderr output from the agent is visible.
    //      Without this, the injector exits immediately, the Python script
    //      finishes, and the console may close or show the prompt — hiding
    //      the agent's output.
    //
    // [RU] Ждём завершения целевого процесса.  Это удерживает инжектор
    //      (и его родителя — Python-скрипт) живыми, поэтому консоль
    //      остаётся открытой и вывод агента в stderr виден.
    //      Без этого инжектор завершается сразу, Python-скрипт заканчивается,
    //      и консоль может закрыться или показать промпт — скрывая
    //      вывод агента.
    WaitForSingleObject(pi.hProcess, INFINITE);

    auto appExitCode = DWORD{ 0 };
    GetExitCodeProcess(pi.hProcess, &appExitCode);
    std::wcout << L"[injector] Target process exited with code "
               << appExitCode << L"." << std::endl;

    return true;
}

int wmain(int argc, wchar_t **argv)
{
    if (argc < 3) {
        std::wcout
            << L"Usage:\n"
            << L"  windows_injector.exe <target.exe> <agent.dll> [extra args...]\n"
            << L"\n"
            << L"  <agent.dll> must be an absolute path.\n"
            << L"\n"
            << L"Example:\n"
            << L"  windows_injector.exe qt_widgets_app.exe "
               L"C:\\project\\builds\\msvc\\lib\\agent.dll\n"
            << std::endl;
        return 1;
    }

    const auto target = std::wstring{ argv[1] };
    const auto dll    = std::wstring{ argv[2] };

    auto args = std::wstring{};
    for (int i = 3; i < argc; ++i) {
        if (i > 3)
            args += L' ';
        args += argv[i];
    }

    return inject(target, dll, args) ? 0 : 2;
}

#else
// Stub so the file can be compiled on non-Windows without errors
int main()
{
    return 0;
}
#endif // _WIN32
