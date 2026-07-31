#include "imageutils.h"

#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

#include <limits>
#include <memory>

#ifdef Q_OS_WIN
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#endif

ImageUtils::ImageUtils(QObject *parent)
    : QObject(parent),
      m_backgroundNetworkManager(this)
{
}

void ImageUtils::saveImageToFile(const QString &imageUrl, const QUrl &localPath)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->get(QNetworkRequest(QUrl(imageUrl)));

    connect(reply, &QNetworkReply::finished, this, [this, manager, reply, localPath]() {
        if (reply->error() == QNetworkReply::NoError) {
            const QString filePath = localPath.toLocalFile();
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly)) {
                const QByteArray payload = reply->readAll();
                const bool written = file.write(payload) == payload.size();
                file.close();
                if (written && file.error() == QFileDevice::NoError) {
                    emit saveCompleted(true, filePath);
                }
                else {
                    emit saveCompleted(false, tr("Unable to write file"));
                }
            }
            else {
                emit saveCompleted(false, tr("Unable to write file"));
            }
        }
        else {
            emit saveCompleted(false, reply->errorString());
        }

        reply->deleteLater();
        manager->deleteLater();
    });
}

void ImageUtils::fetchAndSaveRandomBackground(const QString &apiUrl)
{
    if (m_backgroundFetchInProgress) {
        emit backgroundBusy();
        return;
    }

    m_backgroundApiUrl = QUrl(apiUrl);
    if (!m_backgroundApiUrl.isValid()) {
        emit backgroundError(tr("Invalid background image URL"));
        return;
    }

    m_backgroundAttempt = 0;
    m_backgroundFetchInProgress = true;
    startBackgroundRequest();
}

void ImageUtils::startBackgroundRequest()
{
    ++m_backgroundAttempt;

    QNetworkRequest request(m_backgroundApiUrl);
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                       "AppleWebKit/537.36 (KHTML, like Gecko) "
                                       "Chrome/125.0.0.0 Safari/537.36");
    request.setRawHeader("Accept", "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8");
    request.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    request.setRawHeader("Referer", m_backgroundApiUrl.toEncoded());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(15000);
#endif

    QNetworkReply *reply = m_backgroundNetworkManager.get(request);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QTimer::singleShot(15000, reply, [reply]() {
        if (reply->isRunning()) {
            reply->abort();
        }
    });
#endif
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            const QString errorMessage = reply->errorString();
            reply->deleteLater();
            retryOrFailBackground(errorMessage);
            return;
        }

        const QByteArray imageData = reply->readAll();
        reply->deleteLater();
        if (imageData.size() < 1024) {
            retryOrFailBackground(tr("Background server returned an empty response"));
            return;
        }

        const auto result = std::make_shared<QString>();
        QThread *worker = QThread::create([result, imageData]() {
            *result = ImageUtils::decodeAndSaveBackground(imageData);
        });
        connect(worker, &QThread::finished, this, [this, result]() {
            if (result->isEmpty()) {
                retryOrFailBackground(tr("Unable to decode background image"));
                return;
            }

            m_backgroundFetchInProgress = false;
            emit backgroundReady(*result);
        });
        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
    });
}

void ImageUtils::retryOrFailBackground(const QString &errorMessage)
{
    qWarning() << "fetchAndSaveRandomBackground: attempt" << m_backgroundAttempt
               << "failed:" << errorMessage;
    if (m_backgroundAttempt < 3) {
        QTimer::singleShot(500 * m_backgroundAttempt, this, [this]() {
            startBackgroundRequest();
        });
        return;
    }

    m_backgroundFetchInProgress = false;
    emit backgroundError(errorMessage);
}

QString ImageUtils::decodeAndSaveBackground(const QByteArray &imageData)
{
    QByteArray decodableData = imageData;
    QImage image;
    if (!image.loadFromData(decodableData)) {
        decodableData = convertToJpeg(decodableData);
        if (decodableData.isEmpty() || !image.loadFromData(decodableData)) {
            return QString();
        }
    }

    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
            "/backgrounds";
    QDir().mkpath(cacheDir);
    const QString filePath = cacheDir + "/background_" +
            QString::number(QDateTime::currentMSecsSinceEpoch()) + ".jpg";
    if (!image.save(filePath, "JPEG", 90)) {
        return QString();
    }

    QDir bgDir(cacheDir);
    QStringList filters;
    filters << "background_*.*";
    const QFileInfoList oldFiles = bgDir.entryInfoList(filters, QDir::Files, QDir::Time);
    for (const QFileInfo &oldFile : oldFiles) {
        if (oldFile.absoluteFilePath() != filePath) {
            QFile::remove(oldFile.absoluteFilePath());
        }
    }
    return filePath;
}

QByteArray ImageUtils::convertToJpeg(const QByteArray &imageData)
{
    QImage image;
    if (image.loadFromData(imageData)) {
        QByteArray result;
        QBuffer buffer(&result);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "JPEG", 90);
        return result;
    }

#ifdef Q_OS_WIN
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialize = SUCCEEDED(comResult);

    QByteArray result;
    IWICImagingFactory *factory = nullptr;
    IWICStream *stream = nullptr;
    IWICBitmapDecoder *decoder = nullptr;
    IWICBitmapFrameDecode *frame = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr)) goto cleanup;

    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) goto cleanup;

    hr = stream->InitializeFromMemory(reinterpret_cast<BYTE *>(const_cast<char *>(imageData.data())),
                                      imageData.size());
    if (FAILED(hr)) goto cleanup;

    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) goto cleanup;

    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) goto cleanup;

    {
        UINT width = 0;
        UINT height = 0;
        frame->GetSize(&width, &height);

        const qint64 stride = static_cast<qint64>(width) * 4;
        const qint64 bufferSize = stride * static_cast<qint64>(height);
        static constexpr qint64 kMaximumDecodedBytes = 512LL * 1024 * 1024;
        if (width == 0 || height == 0 ||
                width > static_cast<UINT>((std::numeric_limits<int>::max)()) ||
                height > static_cast<UINT>((std::numeric_limits<int>::max)()) ||
                stride > (std::numeric_limits<UINT>::max)() ||
                bufferSize <= 0 || bufferSize > kMaximumDecodedBytes ||
                bufferSize > (std::numeric_limits<UINT>::max)()) {
            goto cleanup;
        }

        IWICFormatConverter *converter = nullptr;
        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr)) goto cleanup;

        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) {
            converter->Release();
            goto cleanup;
        }

        QByteArray pixels(static_cast<qsizetype>(bufferSize), 0);
        hr = converter->CopyPixels(nullptr, static_cast<UINT>(stride),
                                   static_cast<UINT>(bufferSize),
                                   reinterpret_cast<BYTE *>(pixels.data()));
        converter->Release();
        if (FAILED(hr)) goto cleanup;

        QImage wicImage(reinterpret_cast<const uchar *>(pixels.constData()),
                        static_cast<int>(width), static_cast<int>(height),
                        static_cast<qsizetype>(stride), QImage::Format_ARGB32);
        QBuffer buffer(&result);
        buffer.open(QIODevice::WriteOnly);
        wicImage.save(&buffer, "JPEG", 90);
    }

cleanup:
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();
    if (shouldUninitialize) CoUninitialize();
    return result;
#else
    qWarning() << "convertToJpeg: unsupported image format. Install qtimageformats for WebP support.";
    return QByteArray();
#endif
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

    const QFileInfo fileInfo(cachePath);
    const qint64 twentyFourHours = 24 * 60 * 60;
    return fileInfo.lastModified().secsTo(QDateTime::currentDateTime()) < twentyFourHours &&
           fileInfo.size() > 1024 &&
           fileInfo.suffix().toLower() == "jpg";
}

bool ImageUtils::validateExtension(const QString &filePath)
{
    static const QStringList allowedExtensions = {"jpg", "jpeg", "png", "bmp"};
    const QString extension = QFileInfo(filePath).suffix().toLower();
    return !extension.isEmpty() && allowedExtensions.contains(extension);
}
