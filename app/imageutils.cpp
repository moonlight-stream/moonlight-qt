#include "imageutils.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QEventLoop>
#include <QFileInfo>
#include <QDateTime>

ImageUtils::ImageUtils(QObject *parent) : QObject(parent)
{
}

void ImageUtils::saveImageToFile(const QString &imageUrl, const QUrl &localPath)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request((QUrl(imageUrl)));

    QNetworkReply *reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, [=]()
            {
        if(reply->error() == QNetworkReply::NoError) {
            QString filePath = localPath.toLocalFile();
            QFile file(filePath);
            if(file.open(QIODevice::WriteOnly)) {
                file.write(reply->readAll());
                file.close();
                emit saveCompleted(true, filePath);
            } else {
                emit saveCompleted(false, "无法写入文件");
            }
        } else {
            emit saveCompleted(false, reply->errorString());
        }
        reply->deleteLater();
        manager->deleteLater(); });
}

QString ImageUtils::saveImageFromUrl(const QString &url)
{
    QNetworkAccessManager manager;
    QEventLoop loop;

    QNetworkRequest request(url);
    QNetworkReply *reply = manager.get(request);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError)
    {
        reply->deleteLater();
        return QString();
    }

    QByteArray imageData = reply->readAll();
    reply->deleteLater();

    // 创建缓存目录
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/backgrounds";
    QDir().mkpath(cacheDir);

    // 生成文件名
    QString filePath = cacheDir + "/background.jpg";

    // 保存文件
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(imageData);
        file.close();
        return filePath;
    }

    return QString();
}

bool ImageUtils::fileExists(const QString &path)
{
    return QFileInfo::exists(path);
}

bool ImageUtils::isValidCache(const QString &cachePath)
{
    if (!fileExists(cachePath)) {
        return false;
    }

    QFileInfo fileInfo(cachePath);
    const qint64 twentyFourHours = 24 * 60 * 60;  // 24小时有效期
    
    // 检查文件时效性和有效性
    return fileInfo.lastModified().secsTo(QDateTime::currentDateTime()) < twentyFourHours &&
           fileInfo.size() > 1024 &&          // 文件至少1KB
           fileInfo.suffix().toLower() == "jpg"; // 验证文件格式
}

bool ImageUtils::validateExtension(const QString &filePath)
{
    static const QStringList allowedExtensions = {"jpg", "jpeg", "png", "bmp"};
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    return !extension.isEmpty() && allowedExtensions.contains(extension);
}