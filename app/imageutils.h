#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>

class ImageUtils : public QObject
{
    Q_OBJECT
public:
    explicit ImageUtils(QObject *parent = nullptr);
    
    Q_INVOKABLE void saveImageToFile(const QString &imageUrl, const QUrl &localPath);
    // Returns false when another background request is already in flight. The
    // caller can then coalesce the refresh and retry after the active request.
    Q_INVOKABLE bool fetchAndSaveRandomBackground(const QString &apiUrl);
    Q_INVOKABLE bool fileExists(const QString &path);
    Q_INVOKABLE bool isValidCache(const QString &cachePath);
    Q_INVOKABLE bool validateExtension(const QString &filePath);
    // Returns an empty string when the file is suitable for a local background,
    // otherwise a translated explanation that can be shown directly by QML.
    Q_INVOKABLE QString validateLocalBackgroundImage(const QString &fileUrl);

private:
    void startBackgroundRequest();
    void retryOrFailBackground(const QString &errorMessage);
    static QString decodeAndSaveBackground(const QByteArray &imageData);
    static QByteArray convertToJpeg(const QByteArray &imageData);

    QNetworkAccessManager m_backgroundNetworkManager;
    QUrl m_backgroundApiUrl;
    int m_backgroundAttempt = 0;
    bool m_backgroundFetchInProgress = false;

signals:
    void saveCompleted(bool success, const QString &message);
    void backgroundReady(const QString &filePath);
    void backgroundError(const QString &errorMessage);
    void backgroundBusy();
};

#endif // IMAGEUTILS_H
