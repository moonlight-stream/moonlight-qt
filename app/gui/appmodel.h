#pragma once

#include "backend/boxartmanager.h"
#include "backend/computermanager.h"
#include "streaming/session.h"

#include <QAbstractListModel>

class AppModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        NameRole = Qt::UserRole,
        RunningRole,
        BoxArtRole,
        HiddenRole,
        AppIdRole,
        DirectLaunchRole,
        AppCollectorGameRole,
    };
    Q_ENUM(Roles)

    explicit AppModel(QObject *parent = nullptr);

    // Must be called before any QAbstractListModel functions
    Q_INVOKABLE void initialize(ComputerManager* computerManager, int computerIndex, bool showHiddenGames);

    Q_INVOKABLE Session* createSessionForApp(int appIndex);

    Q_INVOKABLE QVariantList getDisplayList();

    Q_INVOKABLE int getDirectLaunchAppIndex();

    Q_INVOKABLE int getRunningAppId();

    Q_INVOKABLE QString getRunningAppName();

    Q_INVOKABLE void quitRunningApp();

    Q_INVOKABLE void setAppHidden(int appIndex, bool hidden);

    Q_INVOKABLE void setAppDirectLaunch(int appIndex, bool directLaunch);

    Q_INVOKABLE QVariantList getConnectionAddresses();

    // 地址选择框的条目列表。PcView 和 AppView 用同一个 QML 组件
    // （SelectAddressDialog），所以条目形状和「哪一项算选中」的判定必须一致 ——
    // 放在一处，ComputerModel 也调这里。
    //
    // 翻译上下文特意留在 AppModel：这几个字符串在 AppModel 下已经有译文了，
    // 换个上下文会让它们退回英文。
    static QVariantList buildConnectionAddressList(NvComputer* computer);

    Q_INVOKABLE bool hasMultipleConnectionAddresses();

    Q_INVOKABLE bool setActiveAddress(QString address, int port);

    // Undo a setActiveAddress() pin and go back to automatic selection.
    Q_INVOKABLE bool resetToAutomaticAddress();

    Q_INVOKABLE QVariantMap getActiveAddressInfo();

    // Defensive self-heal: re-read m_Computer->currentGameId and force-emit
    // RunningRole dataChanged for the affected rows if it differs from our
    // cached m_CurrentGameId. Useful when the polling thread updated state
    // through a path that bypassed the computerStateChanged signal (e.g.
    // mDNS re-resolution folding via PendingAddTask under contended locks).
    // Cheap when nothing has changed.
    Q_INVOKABLE void forceSyncCurrentGame();

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

private slots:
    void handleComputerStateChanged(NvComputer* computer);

    void handleBoxArtLoaded(NvComputer* computer, NvApp app, QUrl image);

signals:
    void computerLost();

private:
    void updateAppList(QVector<NvApp> newList);

    QVector<NvApp> getVisibleApps(const QVector<NvApp>& appList);

    bool isAppCurrentlyVisible(const NvApp& app);

    NvComputer* m_Computer;
    BoxArtManager m_BoxArtManager;
    ComputerManager* m_ComputerManager;
    QVector<NvApp> m_VisibleApps, m_AllApps;
    int m_CurrentGameId;
    bool m_ShowHiddenGames;
};
