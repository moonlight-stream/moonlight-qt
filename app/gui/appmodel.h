#pragma once

#include "backend/boxartmanager.h"
#include "backend/computermanager.h"
#include "streaming/session.h"

#include <QAbstractListModel>

class AppModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool hasHiddenApps READ hasHiddenApps NOTIFY hasHiddenAppsChanged)
    Q_PROPERTY(bool showHiddenApps READ showHiddenApps WRITE setShowHiddenApps NOTIFY showHiddenAppsChanged)

    enum Roles
    {
        NameRole = Qt::UserRole,
        RunningRole,
        BoxArtRole,
        HiddenRole,
        AppIdRole,
        DirectLaunchRole,
        AppCollectorGameRole,
        DesktopRole,
    };

public:
    explicit AppModel(QObject *parent = nullptr);
    bool hasHiddenApps() const;
    bool showHiddenApps() const { return m_ShowHiddenGames; }
    void setShowHiddenApps(bool show);

    // Must be called before any QAbstractListModel functions
    Q_INVOKABLE void initialize(ComputerManager* computerManager, const QString& computerUuid, bool showHiddenGames);

    void initializeForComputer(ComputerManager* manager, NvComputer* computer, bool showHiddenGames);

    Q_INVOKABLE Session* createSessionForApp(int appIndex);

    Q_INVOKABLE Session* createSessionForAppId(int appId);

    Q_INVOKABLE int getDirectLaunchAppIndex();

    Q_INVOKABLE int getRunningAppId();

    Q_INVOKABLE QString getRunningAppName();

    Q_INVOKABLE void quitRunningApp();

    Q_INVOKABLE void setAppHidden(int appIndex, bool hidden);

    Q_INVOKABLE void setAppDirectLaunch(int appIndex, bool directLaunch);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

private slots:
    void handleComputerStateChanged(NvComputer* computer);

    void handleBoxArtLoaded(NvComputer* computer, NvApp app, QUrl image);

signals:
    void computerLost();
    void hasHiddenAppsChanged();
    void showHiddenAppsChanged();

private:
    void updateAppList(QVector<NvApp> newList);

    QVector<NvApp> getVisibleApps(const QVector<NvApp>& appList);

    NvComputer* m_Computer = nullptr;
    BoxArtManager m_BoxArtManager;
    ComputerManager* m_ComputerManager;
    QVector<NvApp> m_VisibleApps, m_AllApps;
    int m_CurrentGameId = 0;
    bool m_ShowHiddenGames = false;
};
