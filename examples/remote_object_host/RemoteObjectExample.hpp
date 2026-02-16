#pragma once

#include <QObject>
#include <iostream>

#include "rep_RemoteObjectExample_source.h"

class ROExample : public ROExampleSimpleSource
{
    Q_OBJECT
public:
    explicit ROExample(QObject *parent = nullptr)
        : ROExampleSimpleSource(parent)
    {}

    void pong(QString reply) override
    {
        std::cout << "[HOST] received pong from agent: "
                  << reply.toStdString() << std::endl;
    }
};
