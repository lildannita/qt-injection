#!/usr/bin/env python3

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent
BUILDS_DIR = ROOT_DIR / "builds"
APPS = {
    "1": ("qt_widgets_app", "Qt Widgets application"),
    "2": ("qt_quick_app",   "Qt Quick / QML application"),
}
OS_NAME = platform.system()  # "Windows", "Linux", "Darwin"

def ask_choice(prompt: str, options: dict[str, str]) -> str:
    """Print a numbered menu and return the selected key."""
    print(f"\n{prompt}")
    for key, desc in options.items():
        print(f"  [{key}] {desc}")
    while True:
        ans = input(">>> ").strip()
        if ans in options:
            return ans
        print(f"  Invalid choice. Enter one of: {', '.join(options)}")


def find_qt_bin_dir(build_dir: Path) -> Path | None:
    """Find Qt's bin directory by parsing CMakeCache.txt for Qt5Core_DIR."""
    cache = build_dir / "CMakeCache.txt"
    if not cache.is_file():
        return None
    with open(cache, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            # Qt5Core_DIR:PATH=D:/Qt/5.15.18/mingw81_64/lib/cmake/Qt5Core
            if line.startswith("Qt5Core_DIR:PATH="):
                qt_core_dir = Path(line.split("=", 1)[1].strip())
                # .../lib/cmake/Qt5Core  →  .../bin
                qt_bin = qt_core_dir.parent.parent.parent / "bin"
                if qt_bin.is_dir():
                    return qt_bin
    return None


def detect_compilers() -> list[str]:
    """Return a list of compiler preset names already present in builds/."""
    if not BUILDS_DIR.is_dir():
        return []
    return sorted(
        d.name for d in BUILDS_DIR.iterdir()
        if d.is_dir() and (d / "bin").is_dir()
    )


def guess_compiler_name() -> str:
    """Return a sensible default build-dir name for the current platform."""
    if OS_NAME == "Windows":
        # Prefer MSVC if cl.exe is found, otherwise mingw.
        try:
            subprocess.run(["cl"], capture_output=True)
            return "msvc"
        except FileNotFoundError:
            return "mingw"
    elif OS_NAME == "Darwin":
        return "clang"
    else:
        return "gcc"


def cmake_generator_args(compiler: str) -> list[str]:
    """Return extra CMake configure args for a given compiler preset."""
    if shutil.which("ninja"):
        return ["-G", "Ninja"]
    if OS_NAME == "Windows":
        if compiler == "msvc":
            return ["-G", "NMake Makefiles"]
        if compiler == "mingw":
            return ["-G", "MinGW Makefiles"]
    return []  # Unix default (Make)


def cmake_build_cmd() -> list[str]:
    """Return the build command (cmake --build . or equivalent)."""
    return ["cmake", "--build", ".", "--parallel"]


def build_project(compiler: str) -> Path:
    """Configure + build the project. Returns the build directory Path."""
    build_dir = BUILDS_DIR / compiler
    build_dir.mkdir(parents=True, exist_ok=True)

    # clear cache
    cache_file = build_dir / "CMakeCache.txt"
    if cache_file.is_file():
        expected_gen = "Ninja" if shutil.which("ninja") else None
        with open(cache_file, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                if line.startswith("CMAKE_GENERATOR:"):
                    cached_gen = line.split("=", 1)[1].strip()
                    if expected_gen and cached_gen != expected_gen:
                        print(f"  Generator changed ({cached_gen} → {expected_gen}), clearing cache...")
                        shutil.rmtree(build_dir)
                        build_dir.mkdir(parents=True, exist_ok=True)
                    break

    # configure
    configure_cmd = [
        "cmake",
        *cmake_generator_args(compiler),
        f"-DCMAKE_BUILD_TYPE=Debug",
        str(ROOT_DIR),
    ]
    print(f"\n=== CMake configure ({compiler}) ===")
    print(f"  dir:  {build_dir}")
    print(f"  cmd:  {' '.join(configure_cmd)}")
    result = subprocess.run(configure_cmd, cwd=build_dir)
    if result.returncode != 0:
        print("ERROR: CMake configure failed.", file=sys.stderr)
        sys.exit(1)

    # build
    build_cmd = cmake_build_cmd()
    print(f"\n=== CMake build ({compiler}) ===")
    print(f"  cmd:  {' '.join(build_cmd)}")
    result = subprocess.run(build_cmd, cwd=build_dir)
    if result.returncode != 0:
        print("ERROR: CMake build failed.", file=sys.stderr)
        sys.exit(1)

    return build_dir


def find_agent_lib(build_dir: Path) -> Path:
    """Locate the agent shared library in the build tree."""
    lib_dir = build_dir / "lib"
    for pattern in ("agent.dll", "libagent.so", "libagent.dylib"):
        candidates = list(lib_dir.glob(pattern))
        if candidates:
            return candidates[0]
    # Fallback: search recursively.
    for ext in (".dll", ".so", ".dylib"):
        for p in build_dir.rglob(f"*agent*{ext}"):
            return p
    print("ERROR: Cannot find agent library in build tree.", file=sys.stderr)
    sys.exit(1)


def find_app_exe(build_dir: Path, app_name: str) -> Path:
    """Locate the application executable in the build tree."""
    bin_dir = build_dir / "bin"
    if OS_NAME == "Windows":
        p = bin_dir / f"{app_name}.exe"
    else:
        p = bin_dir / app_name
    if p.exists():
        return p
    # Fallback: search recursively.
    suffix = ".exe" if OS_NAME == "Windows" else ""
    for p in build_dir.rglob(f"{app_name}{suffix}"):
        if p.is_file():
            return p
    print(f"ERROR: Cannot find {app_name} executable.", file=sys.stderr)
    sys.exit(1)


def find_injector(build_dir: Path) -> Path:
    """Locate windows_injector.exe."""
    p = build_dir / "bin" / "windows_injector.exe"
    if p.exists():
        return p
    for p in build_dir.rglob("windows_injector.exe"):
        return p
    print("ERROR: Cannot find windows_injector.exe.", file=sys.stderr)
    sys.exit(1)


def launch(build_dir: Path, app_name: str):
    """Launch the chosen application with the agent injected."""
    agent = find_agent_lib(build_dir)
    app   = find_app_exe(build_dir, app_name)

    print(f"\n=== Launching ===")
    print(f"  App  : {app}")
    print(f"  Agent: {agent}")

    if OS_NAME == "Linux":
        env = os.environ.copy()
        env["LD_PRELOAD"] = str(agent)
        print(f"  Method: LD_PRELOAD={agent}")
        subprocess.run([str(app)], env=env)

    elif OS_NAME == "Darwin":
        env = os.environ.copy()
        env["DYLD_INSERT_LIBRARIES"] = str(agent)
        print(f"  Method: DYLD_INSERT_LIBRARIES={agent}")
        subprocess.run([str(app)], env=env)

    elif OS_NAME == "Windows":
        injector = find_injector(build_dir)
        print(f"  Injector: {injector}")
        print(f"  Method: CreateRemoteThread + LoadLibraryW")

        # Ensure Qt DLLs are findable by the target process.
        # The agent depends on Qt5Core.dll etc., which must be in PATH
        # when LoadLibraryW resolves dependencies inside the suspended process.
        env = os.environ.copy()
        qt_bin = find_qt_bin_dir(build_dir)
        if qt_bin:
            env["PATH"] = str(qt_bin) + os.pathsep + env.get("PATH", "")
            print(f"  Qt bin:  {qt_bin}")

        subprocess.run([str(injector), str(app), str(agent.resolve())], env=env)

    else:
        print(f"ERROR: Unsupported OS '{OS_NAME}'.", file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Build & run the Qt injection test stand.")
    parser.add_argument("--app", choices=["widgets", "quick"], help="Skip app selection prompt.")
    parser.add_argument("--compiler", help="Skip compiler selection prompt.")
    parser.add_argument("--no-build", action="store_true", help="Skip build step (use existing).")
    args = parser.parse_args()

    # 1. Choose compiler / build dir.
    if args.compiler:
        compiler = args.compiler
    else:
        existing = detect_compilers()
        if len(existing) > 1:
            opts = {str(i + 1): c for i, c in enumerate(existing)}
            opts[str(len(existing) + 1)] = f"New build ({guess_compiler_name()})"
            choice = ask_choice("Several builds found. Choose one:", opts)
            idx = int(choice) - 1
            if idx < len(existing):
                compiler = existing[idx]
            else:
                compiler = guess_compiler_name()
        elif len(existing) == 1:
            compiler = existing[0]
            print(f"Using existing build: {compiler}")
        else:
            compiler = guess_compiler_name()
            print(f"No existing builds. Will create: builds/{compiler}")

    # 2. Build.
    if not args.no_build:
        build_dir = build_project(compiler)
    else:
        build_dir = BUILDS_DIR / compiler
        if not build_dir.is_dir():
            print(f"ERROR: Build directory {build_dir} does not exist.", file=sys.stderr)
            sys.exit(1)

    # 3. Choose app.
    if args.app:
        app_name = "qt_widgets_app" if args.app == "widgets" else "qt_quick_app"
    else:
        choice = ask_choice("Which application to launch?",
                            {k: v[1] for k, v in APPS.items()})
        app_name = APPS[choice][0]

    # 4. Launch.
    launch(build_dir, app_name)


if __name__ == "__main__":
    main()
