#include "appmodel.h"
#include "backend/computermanager.h"
#include "streaming/session.h"

#include <QAbstractListModel>

class ComputerModel : public QAbstractListModel
{
    Q_OBJECT

    enum Roles
    {
        NameRole = Qt::UserRole,
        OnlineRole,
        PairedRole,
        BusyRole,
        WakeableRole,
        StatusUnknownRole,
        ServerSupportedRole,
        DetailsRole,
        PreviewRole,
        UuidRole,
        StreamWindowStateRole,
        StreamProgressTextRole
    };

public:
    enum StreamWindowState { NoStreamWindow, OpeningStreamWindow, ConnectedStreamWindow };
    Q_ENUM(StreamWindowState)

    explicit ComputerModel(QObject* object = nullptr);

    // Must be called before any QAbstractListModel functions
    Q_INVOKABLE void initialize(ComputerManager* computerManager);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void deleteComputer(int computerIndex);
    Q_INVOKABLE int indexOfUuid(const QString& uuid) const;

    Q_INVOKABLE QString generatePinString();

    Q_INVOKABLE void pairComputer(int computerIndex, QString pin);

    Q_INVOKABLE void testConnectionForComputer(int computerIndex);

    Q_INVOKABLE void wakeComputer(int computerIndex);

    Q_INVOKABLE void renameComputer(int computerIndex, QString name);

    Q_INVOKABLE void initializeQuickApps(AppModel* apps, int computerIndex);
    Q_INVOKABLE Session* createQuickSession(int computerIndex);
    Q_INVOKABLE void refreshDesktopPreviews();

signals:
    void pairingCompleted(QVariant error);
    void connectionTestCompleted(int result, QString blockedPorts);

private slots:
    void handleComputerStateChanged(NvComputer* computer);

    void handlePairingCompleted(NvComputer* computer, QString error);

private:
    QVector<NvComputer*> m_Computers;
    ComputerManager* m_ComputerManager;
#ifdef Q_OS_WIN
    void refreshStreamWindows();
    struct StreamStatus {
        StreamWindowState state = NoStreamWindow;
        QString progressText;
    };
    QHash<QString, StreamStatus> m_StreamWindows;
#endif
};
