#pragma once

#include <QObject>

class ComputerManager;
class NvComputer;
class Session;
class StreamingPreferences;

namespace CliStartStream
{

class Event;
class LauncherPrivate;

class Launcher : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE_D(m_DPtr, Launcher)

public:
    explicit Launcher(QString computer, QString app,
                      StreamingPreferences* preferences,
                      QObject *parent = nullptr);
    ~Launcher();
    Q_INVOKABLE void execute(ComputerManager *manager);

signals:
    void searchingComputer(QString name);
    void searchingApp(QString name);
    void sessionCreated(QString appName, Session *session);
    void failed(QString text);

private slots:
    void onComputerUpdated(NvComputer *computer);
    void onTimeout();

private:
    QScopedPointer<LauncherPrivate> m_DPtr;
};

}
