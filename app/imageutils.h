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
    Q_INVOKABLE void fetchAndSaveRandomBackground(const QString &apiUrl);
    Q_INVOKABLE bool fileExists(const QString &path);
    Q_INVOKABLE bool isValidCache(const QString &cachePath);
    Q_INVOKABLE bool validateExtension(const QString &filePath);

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
