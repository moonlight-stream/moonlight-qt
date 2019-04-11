#pragma once

#include <QObject>

class I3WindowManager : public QObject
{
    Q_OBJECT

public:
    I3WindowManager();

    Q_PROPERTY(bool isRunningI3 MEMBER isRunningI3 CONSTANT)

    Q_INVOKABLE void start();
    Q_INVOKABLE void hideWindow();
    Q_INVOKABLE void showWindow();

private:
    bool isRunningI3;
    QString mark;
    quint64 appId;
};

