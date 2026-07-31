#pragma once

#include <QObject>
#include <QNetworkAccessManager>

class QFile;
class QNetworkReply;

// 应用内更新的下载与安装。
//
// 名字里的 "Portable" 是历史包袱：这个类最早只服务 Windows 便携版。现在 macOS 的
// .app 安装也走同一套骨架（下载 → 校验 → 甩一个 detached 脚本换掉自己并重启），
// 只是每一步的平台实现不同。信号名和 QML 里的调用点都还在用旧名字，没跟着改。
class PortableUpdateInstaller : public QObject
{
    Q_OBJECT
public:
    explicit PortableUpdateInstaller(QObject *parent = nullptr);

    bool supportsInAppUpdate() const;
    void installUpdate(const QString& url, const QString& expectedDigest = QString());

signals:
    void onPortableUpdateStatusChanged(QString message);
    void onPortableUpdateFailed(QString message);

private slots:
    void handlePortableUpdateMetaDataChanged();
    void handlePortableUpdateDownloadReadyRead();
    void handlePortableUpdateDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void handlePortableUpdateDownloadFinished();

private:
    bool isPortableInstall() const;
    bool isBundleInstall() const;
    // macOS：当前运行的 Moonlight.app 的路径，取不到（比如没在 bundle 里跑）时为空
    QString getInstalledBundlePath() const;
    QString getUpdateArchiveName() const;
    QString getUpdateArchiveSuffix() const;
    // 磁盘空间探测和工作目录落在哪个卷上，Windows 是安装目录、macOS 是缓存目录
    QString getUpdateStorageProbePath() const;
    QString getPortableUpdaterExecutable() const;
    bool ensureWritableInstallDir(QString& errorMessage) const;
    QString createPortableUpdateWorkspace() const;
    QString materializePortableUpdateScript(const QString& workspace) const;
    // macOS：挂载 DMG、把里面的 Moonlight.app 拷进工作目录、清掉隔离属性
    bool stageMacUpdateBundle(const QString& archivePath,
                              QString& stagedBundlePath,
                              QString& errorMessage);
    bool runTool(const QString& program, const QStringList& arguments, int timeoutMs) const;
    bool ensureSufficientDiskSpace(qint64 requiredBytes, QString& errorMessage) const;
    bool verifyUpdateArchive(const QString& archivePath, QString& errorMessage) const;
    qint64 estimateRequiredWorkspaceBytes(qint64 archiveBytes) const;
    void resetPortableUpdateState(bool removeWorkspace);

    QNetworkAccessManager* m_UpdateNam;
    QNetworkReply* m_UpdateReply;
    QFile* m_UpdateFile;
    QString m_PortableUpdateWorkspace;
    QString m_PortableUpdateError;
    QByteArray m_ExpectedSha256;
    bool m_MetadataChecked;
};
