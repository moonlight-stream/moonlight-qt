#include "boxartmanager.h"

#include <QStandardPaths>
#include <QImageReader>
#include <QImageWriter>

BoxArtManager::BoxArtManager(QObject *parent) :
    QObject(parent),
    m_BoxArtDir(
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/boxart"),
    m_PlaceholderImage(":/res/no_app_image.png")
{
    if (!m_BoxArtDir.exists()) {
        m_BoxArtDir.mkpath(".");
    }
}

QString
BoxArtManager::getFilePathForBoxArt(NvComputer* computer, int appId)
{
    QDir dir = m_BoxArtDir;

    // Create the cache directory if it did not already exist
    if (!dir.exists(computer->uuid)) {
        dir.mkdir(computer->uuid);
    }

    // Change to this computer's box art cache folder
    dir.cd(computer->uuid);

    // Try to open the cached file
    return dir.filePath(QString::number(appId) + ".png");
}

QImage BoxArtManager::loadBoxArt(NvComputer* computer, NvApp& app)
{
    // Try to open the cached file
    QFile cacheFile(getFilePathForBoxArt(computer, app.id));
    if (cacheFile.open(QFile::ReadOnly)) {
        // Return what we have if it's a valid image
        QImage image = QImageReader(&cacheFile).read();
        if (!image.isNull()) {
            return image;
        }
    }

    // If we get here, we need to fetch asynchronously.
    // Kick off a worker on our thread pool to do just that.
    NetworkBoxArtLoadTask* netLoadTask = new NetworkBoxArtLoadTask(this, computer, app);
    QThreadPool::globalInstance()->start(netLoadTask);

    // Return the placeholder then we can notify the caller
    // later when the real image is ready.
    return m_PlaceholderImage;
}

void BoxArtManager::handleBoxArtLoadComplete(NvComputer* computer, NvApp app, QImage image)
{
    emit boxArtLoadComplete(computer, app, image);
}

QImage BoxArtManager::loadBoxArtFromNetwork(NvComputer* computer, int appId)
{
    NvHTTP http(computer->activeAddress);

    QImage image;
    try {
        image = http.getBoxArt(appId);
    } catch (...) {}

    // Cache the box art on disk if it loaded
    if (!image.isNull()) {
        image.save(getFilePathForBoxArt(computer, appId));
    }

    return image;
}
