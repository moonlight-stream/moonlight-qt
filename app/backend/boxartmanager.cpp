#include "boxartmanager.h"
#include "../path.h"

#include <QImageReader>
#include <QImageWriter>
#include <QMutexLocker>

BoxArtManager::BoxArtManager(QObject *parent) :
    QObject(parent),
    m_BoxArtDir(Path::getBoxArtCacheDir()),
    m_ThreadPool(this)
{
    // 4 is a good balance between fast loading for large
    // app grids and not crushing GFE with tons of requests
    // and causing UI jank from constantly stalling to decode
    // new images.
    m_ThreadPool.setMaxThreadCount(4);
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

bool BoxArtManager::isPlaceholderBoxArt(const QSize& size)
{
    return size == QSize(130, 180) ||
           size == QSize(628, 888) ||
           size == QSize(200, 266);
}

bool BoxArtManager::isCachedPlaceholderBoxArt(const QString& cachePath)
{
    {
        QMutexLocker locker(&m_PlaceholderCacheMutex);
        const auto cached = m_PlaceholderCache.constFind(cachePath);
        if (cached != m_PlaceholderCache.constEnd()) {
            return cached.value();
        }
    }

    const bool isPlaceholder = isPlaceholderBoxArt(QImageReader(cachePath).size());
    rememberPlaceholderBoxArt(cachePath, isPlaceholder);
    return isPlaceholder;
}

void BoxArtManager::rememberPlaceholderBoxArt(const QString& cachePath, bool isPlaceholder)
{
    QMutexLocker locker(&m_PlaceholderCacheMutex);
    m_PlaceholderCache.insert(cachePath, isPlaceholder);
}

class NetworkBoxArtLoadTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    NetworkBoxArtLoadTask(BoxArtManager* boxArtManager, NvComputer* computer, NvApp& app)
        : m_Bam(boxArtManager),
          m_Computer(computer),
          m_App(app)
    {
        connect(this, &NetworkBoxArtLoadTask::boxArtFetchCompleted,
                boxArtManager, &BoxArtManager::handleBoxArtLoadComplete);
    }

signals:
    void boxArtFetchCompleted(NvComputer* computer, NvApp app, QUrl image);

private:
    void run()
    {
        QUrl image = m_Bam->loadBoxArtFromNetwork(m_Computer, m_App);
        if (image.isEmpty()) {
            // Give it another shot if it fails once
            image = m_Bam->loadBoxArtFromNetwork(m_Computer, m_App);
        }
        emit boxArtFetchCompleted(m_Computer, m_App, image);
    }

    BoxArtManager* m_Bam;
    NvComputer* m_Computer;
    NvApp m_App;
};

QUrl BoxArtManager::loadBoxArt(NvComputer* computer, NvApp& app)
{
    // Try to open the cached file if it exists and contains data
    QFile cacheFile(getFilePathForBoxArt(computer, app.id));
    if (cacheFile.exists() && cacheFile.size() > 0) {
        if (!app.isAppCollectorGame && isCachedPlaceholderBoxArt(cacheFile.fileName())) {
            return QUrl("qrc:/res/no_app_image.png");
        }
        return QUrl::fromLocalFile(cacheFile.fileName());
    }

    // If we get here, we need to fetch asynchronously.
    // Kick off a worker on our thread pool to do just that.
    NetworkBoxArtLoadTask* netLoadTask = new NetworkBoxArtLoadTask(this, computer, app);
    m_ThreadPool.start(netLoadTask);

    // Return the placeholder then we can notify the caller
    // later when the real image is ready.
    return QUrl("qrc:/res/no_app_image.png");
}

void BoxArtManager::deleteBoxArt(NvComputer* computer)
{
    QDir dir(Path::getBoxArtCacheDir());

    // Delete everything in this computer's box art directory
    if (dir.cd(computer->uuid)) {
        dir.removeRecursively();
    }
}

void BoxArtManager::handleBoxArtLoadComplete(NvComputer* computer, NvApp app, QUrl image)
{
    if (!image.isEmpty()) {
        emit boxArtLoadComplete(computer, app, image);
    }
}

QUrl BoxArtManager::loadBoxArtFromNetwork(NvComputer* computer, const NvApp& app)
{
    NvHTTP http(computer);

    QString cachePath = getFilePathForBoxArt(computer, app.id);
    QImage image;
    try {
        image = http.getBoxArt(app.id);
    } catch (...) {}

    // Cache the box art on disk if it loaded
    if (!image.isNull()) {
        if (image.save(cachePath)) {
            // Cache only the image property. App classification is applied by
            // the caller so a later classification change cannot poison this
            // path's cached result.
            const bool isPlaceholder = isPlaceholderBoxArt(image.size());
            rememberPlaceholderBoxArt(cachePath, isPlaceholder);
            if (!app.isAppCollectorGame && isPlaceholder) {
                return QUrl("qrc:/res/no_app_image.png");
            }
            return QUrl::fromLocalFile(cachePath);
        }
        else {
            // A failed save() may leave a zero byte file. Make sure that's removed.
            QFile(cachePath).remove();
        }
    }

    return QUrl();
}

#include "boxartmanager.moc"
