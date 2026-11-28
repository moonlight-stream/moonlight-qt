#include "filetransferwindow.h"
#include "filetransferstyle.h"

#include <QAction>
#include <QApplication>
#include <QCache>
#include <QClipboard>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QCloseEvent>
#include <QTimer>
#include <QCollator>
#include <QCheckBox>
#include <QFrame>
#include <QInputDialog>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDrag>
#include <QDropEvent>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QHostInfo>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QScopeGuard>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QToolButton>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <qt_windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <algorithm>
#include <functional>
#include <utility>

class TransferFileIcons final : public QObject
{
public:
    TransferFileIcons(QObject* parent, std::function<void()> refresh)
        : QObject(parent), m_Refresh(std::move(refresh)) { m_Worker.setMaxThreadCount(1); }

    ~TransferFileIcons() override
    {
        m_Worker.clear();
        m_Worker.waitForDone();
    }

    using Key = QPair<QString, qint64>;

    QIcon icon(const Key& key, bool remote)
    {
        if (const auto* cached = m_Cache.object(key)) return *cached;
        if (!m_Pending.contains(key)) {
            m_Pending.insert(key);
            m_Worker.start([this, key, remote] {
                const auto image = shellIcon(key.first, remote);
                QMetaObject::invokeMethod(this, [this, key, image] {
                    m_Pending.remove(key);
                    m_Cache.insert(key, new QIcon(QPixmap::fromImage(image)));
                    m_Refresh();
                }, Qt::QueuedConnection);
            });
        }
        return {};
    }

private:
    static QImage shellIcon(const QString& name, bool remote)
    {
        const auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return {};
        const auto cleanup = qScopeGuard([hr] { if (SUCCEEDED(hr)) CoUninitialize(); });
        SHFILEINFOW info{};
        const UINT flags = SHGFI_ICON | SHGFI_LARGEICON | (remote ? SHGFI_USEFILEATTRIBUTES : 0);
        if (!SHGetFileInfoW(name.toStdWString().c_str(), FILE_ATTRIBUTE_NORMAL, &info, sizeof(info), flags) || !info.hIcon)
            return {};
        const auto image = QImage::fromHICON(info.hIcon);
        DestroyIcon(info.hIcon);
        return image;
    }

    QCache<Key, QIcon> m_Cache{512};
    QSet<Key> m_Pending;
    QThreadPool m_Worker;
    std::function<void()> m_Refresh;
};

namespace {
using FileAction = FileTransferEngine::FileAction;

constexpr char clipboardType[] = "application/x-moonlight-transfer-selection";
constexpr int numericSortRole = Qt::UserRole + 1;

QString nativePath(const QString& path)
{
    return QDir::toNativeSeparators(path);
}

QString taskKey(bool fromRemote, const QString& path, const QString& destination)
{
    auto normalized = [](const QString& name) {
        return QDir::cleanPath(QDir::fromNativeSeparators(name));
    };
    const auto tuple = QJsonDocument(QJsonArray{fromRemote, normalized(path), normalized(destination)});
    return QString::fromLatin1(QCryptographicHash::hash(tuple.toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}

QString byteCounts(const QLocale& locale, qint64 bytes, qint64 total)
{
    const auto done = locale.formattedDataSize(bytes);
    return total < 0 ? done : done + QStringLiteral(" / ") + locale.formattedDataSize(total);
}

void setNumericCell(QTreeWidgetItem* item, int column, const QVariant& value, const QString& text)
{
    item->setData(column, numericSortRole, value);
    item->setText(column, text);
}

class TransferListItem final : public QTreeWidgetItem
{
public:
    TransferListItem(int nameColumn, const TransferEntry& entry, bool remote, TransferFileIcons* icons,
                     bool pinFoldersFirst = false)
        : m_NameColumn(nameColumn), m_PinnedFolder(pinFoldersFirst && entry.directory), m_Remote(remote), m_Icons(icons)
    {
        if (entry.directory) return;
        if (remote) {
            // Only the extension is meaningful on this computer.
            const auto suffix = QFileInfo(entry.name).suffix().toLower();
            if (!suffix.isEmpty()) m_IconKey.first = '.' + suffix;
        } else {
            m_IconKey = {nativePath(entry.path), entry.modified.toMSecsSinceEpoch()};
        }
    }

    QVariant data(int column, int role) const override
    {
        if (role == Qt::DecorationRole && column == m_NameColumn && !m_IconKey.first.isEmpty()) {
            const auto icon = m_Icons->icon(m_IconKey, m_Remote);
            if (!icon.isNull()) return icon;
        }
        return QTreeWidgetItem::data(column, role);
    }

    bool operator<(const QTreeWidgetItem& other) const override
    {
        const auto& rhs = static_cast<const TransferListItem&>(other);
        const int column = treeWidget()->sortColumn();
        if (column < 0) return false;
        if (m_PinnedFolder != rhs.m_PinnedFolder) {
            // Qt reverses the comparison for descending sorts. Keep pinned folders first either way.
            return treeWidget()->header()->sortIndicatorOrder() == Qt::AscendingOrder ? m_PinnedFolder : rhs.m_PinnedFolder;
        }
        static const QCollator collator = [] {
            QCollator result;
            result.setNumericMode(true);
            result.setCaseSensitivity(Qt::CaseInsensitive);
            return result;
        }();
        if (column != m_NameColumn) {
            const auto key = data(column, numericSortRole);
            const auto otherKey = other.data(column, numericSortRole);
            if (key.isValid() != otherKey.isValid()) return !key.isValid();
            if (key != otherKey) return key.toLongLong() < otherKey.toLongLong();
            if (!key.isValid()) {
                const int order = collator.compare(text(column), other.text(column));
                if (order != 0) return order < 0;
            }
        }
        const auto name = text(m_NameColumn);
        const auto otherName = other.text(m_NameColumn);
        const int order = collator.compare(name, otherName);
        return order != 0 ? order < 0 : name < otherName;
    }

private:
    const int m_NameColumn;
    const bool m_PinnedFolder;
    const bool m_Remote;
    TransferFileIcons* const m_Icons;
    TransferFileIcons::Key m_IconKey;
};

QLabel* heading(const QString& text)
{
    auto* label = new QLabel(text);
    label->setTextFormat(Qt::PlainText);
    label->setFont(FileTransferStyle::headingFont(label->font()));
    return label;
}

QToolButton* tool(QHBoxLayout* layout, const QString& iconName, QStyle::StandardPixmap fallback, const QString& text)
{
    auto* button = new QToolButton;
    button->setObjectName("transferTool");
    button->setIcon(FileTransferStyle::icon(button, iconName, fallback));
    button->setToolTip(text);
    button->setAccessibleName(text);
    layout->addWidget(button);
    return button;
}

// Draw checkboxes from the selection model so checks, Ctrl/Shift selection
// and the keyboard shortcuts always operate on exactly the same files.
class SelectionColumn final : public QStyledItemDelegate
{
public:
    SelectionColumn(QCheckBox* indicator, QObject* parent)
        : QStyledItemDelegate(parent), m_Indicator(indicator) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);
        const auto* style = m_Indicator->style();
        QStyleOptionButton check;
        check.initFrom(m_Indicator);
        const QSize size(style->pixelMetric(QStyle::PM_IndicatorWidth), style->pixelMetric(QStyle::PM_IndicatorHeight));
        check.rect = QStyle::alignedRect(option.direction, Qt::AlignCenter, size, option.rect);
        check.state = option.state.testFlag(QStyle::State_Selected) ? QStyle::State_On : QStyle::State_Off;
        check.state |= option.state & (QStyle::State_Enabled | QStyle::State_Active);
        style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &check, painter, m_Indicator);
    }

private:
    QCheckBox* const m_Indicator;
};

class FileList final : public QTreeWidget
{
public:
    explicit FileList(bool remote) : m_Remote(remote)
    {
        setDragDropMode(remote ? DragDrop : DragOnly);
        setDefaultDropAction(Qt::CopyAction);
        setDropIndicatorShown(false);
    }

    std::function<QList<TransferEntry>()> selectedFiles;
    std::function<QString(QPoint)> destinationAt;
    std::function<void(const QList<TransferEntry>&, const QString&)> upload;
    std::function<void(const QList<TransferEntry>&, QPoint)> downloadAt;

protected:
    void startDrag(Qt::DropActions) override
    {
        const auto entries = selectedFiles();
        if (entries.isEmpty()) return;
        if (m_Remote) {
            // Keep remote paths on their own machine. Capture the release and write
            // straight to the selected local folder through the transfer queue.
            m_Dragged = entries;
            viewport()->grabMouse(Qt::DragCopyCursor);
            return;
        }
        QList<QUrl> urls;
        for (const auto& entry : entries) urls << QUrl::fromLocalFile(entry.path);
        auto* mime = new QMimeData;
        mime->setUrls(urls);
        QDrag drag(this);
        drag.setMimeData(mime);
        drag.exec(Qt::CopyAction);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (m_Dragged.isEmpty()) QTreeWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && !m_Dragged.isEmpty()) {
            const auto entries = endDrag();
            downloadAt(entries, event->globalPosition().toPoint());
            event->accept();
        } else {
            QTreeWidget::mouseReleaseEvent(event);
        }
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (!m_Dragged.isEmpty() && event->key() == Qt::Key_Escape) endDrag();
        else QTreeWidget::keyPressEvent(event);
    }

    bool viewportEvent(QEvent* event) override
    {
        if (event->type() == QEvent::UngrabMouse || event->type() == QEvent::Hide) m_Dragged.clear();
        return QTreeWidget::viewportEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent* event) override
    {
        const auto urls = event->mimeData()->urls();
        if (m_Remote && event->possibleActions().testFlag(Qt::CopyAction) && !urls.isEmpty() &&
            std::all_of(urls.cbegin(), urls.cend(), [](const QUrl& url) { return url.isLocalFile(); })) {
            event->setDropAction(Qt::CopyAction);
            event->accept();
        }
    }

    void dragMoveEvent(QDragMoveEvent* event) override
    {
        if (!destinationAt(event->position().toPoint()).isEmpty()) {
            event->setDropAction(Qt::CopyAction);
            event->accept();
        } else event->ignore();
    }

    void dropEvent(QDropEvent* event) override
    {
        const auto folder = destinationAt(event->position().toPoint());
        if (folder.isEmpty() || !event->possibleActions().testFlag(Qt::CopyAction)) return;
        const auto entries = FileTransferEngine::localDropEntries(event->mimeData()->urls());
        if (entries.isEmpty()) return;
        upload(entries, folder);
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && columnAt(event->pos().x()) == 0) {
            if (auto* item = itemAt(event->pos())) {
                const bool checked = item->isSelected();
                setCurrentItem(item, 1, QItemSelectionModel::NoUpdate);
                item->setSelected(!checked);
            }
            return;
        }
        QTreeWidget::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (columnAt(event->pos().x()) != 0)
            QTreeWidget::mouseDoubleClickEvent(event);
    }

private:
    QList<TransferEntry> endDrag()
    {
        auto entries = std::exchange(m_Dragged, {});
        viewport()->releaseMouse();
        setState(NoState);
        return entries;
    }

    const bool m_Remote;
    QList<TransferEntry> m_Dragged;
};

}

FileTransferWindow::FileTransferWindow(QUrl host, QSslCertificate certificate, QByteArray token,
                                       QString hostName, QString hostId, QWidget* parent, bool standalone)
    : QWidget(parent, Qt::Window)
    , m_Engine(new FileTransferEngine(host, certificate, token, this, standalone,
        [this](bool remote, int itemCount, const QList<FileConflict>& conflicts, FileTransferEngine::ConflictReply reply) {
            showFileConflicts(this, remote, itemCount, conflicts, std::move(reply));
        }))
    , m_ClipboardId(QUuid::createUuid().toRfc4122())
    , m_Available(!token.isEmpty())
    , m_Standalone(standalone)
{
    setWindowTitle(tr("File transfer"));
    FileTransferStyle::apply(this);
    resize(1160, 710);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    auto* vertical = new QSplitter(Qt::Vertical);
    auto* browsers = new QSplitter(Qt::Horizontal);
    layout->addWidget(vertical);
    vertical->addWidget(browsers);
    const auto hostKey = hostId.isEmpty() ? host.host() : hostId;
    m_Panes[0].bookmarksKey = QStringLiteral("fileTransfer/bookmarks/local");
    m_Panes[1].bookmarksKey = QStringLiteral("fileTransfer/bookmarks/hosts/") + hostKey;
    const auto directorySettings = QStringLiteral("fileTransfer/lastDirectories/") + hostKey;
    m_Panes[0].lastDirectoryKey = directorySettings + QStringLiteral("/local");
    m_Panes[1].lastDirectoryKey = directorySettings + QStringLiteral("/remote");
    browsers->addWidget(makePane(false, tr("Local · %1").arg(QHostInfo::localHostName())));
    browsers->addWidget(makePane(true, tr("Remote · %1").arg(hostName)));
    browsers->setChildrenCollapsible(false);
    browsers->setSizes({550, 550});

    auto* queuePanel = new QFrame;
    queuePanel->setObjectName("transferPanel");
    auto* queueLayout = new QVBoxLayout(queuePanel);
    queueLayout->setContentsMargins(12, 12, 12, 12);
    queueLayout->setSpacing(8);
    auto* queueBar = new QHBoxLayout;
    queueBar->addWidget(heading(tr("Transfers")));
    queueBar->addStretch();
    m_Resume = new QPushButton(tr("Resume selected"));
    queueBar->addWidget(m_Resume);
    m_Cancel = new QPushButton(tr("Cancel selected"));
    m_Clear = new QPushButton(tr("Clear finished"));
    queueBar->addWidget(m_Cancel);
    queueBar->addWidget(m_Clear);
    queueLayout->addLayout(queueBar);
    m_Queue = new QTreeWidget;
    FileTransferStyle::configureList(m_Queue, {tr("Name"), tr("Status"), tr("Transferred"), tr("Speed"), tr("Source"), tr("Destination")});
    m_Queue->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_Queue->header()->setSortIndicator(-1, Qt::AscendingOrder); // Show enqueue order until a column is chosen.
    m_Queue->setSortingEnabled(true);
    m_Queue->setColumnWidth(1, 150);
    m_Queue->setColumnWidth(2, 130);
    m_Queue->setColumnWidth(3, 96);
    m_Queue->setColumnWidth(4, 190);
    m_Queue->setColumnWidth(5, 190);
    queueLayout->addWidget(m_Queue);
    vertical->addWidget(queuePanel);
    vertical->setChildrenCollapsible(false);
    vertical->setSizes({440, 200});

    connect(m_Engine, &FileTransferEngine::listed, this, &FileTransferWindow::showDirectory);
    m_RemoteRetryTimer.setSingleShot(true);
    connect(&m_RemoteRetryTimer, &QTimer::timeout, this, [this] {
        if (isVisible() && !isMinimized() && m_RemoteRetryDelay)
            m_Engine->list(true, m_Panes[1].requested);
    });
    connect(m_Engine, &FileTransferEngine::progress, this, &FileTransferWindow::showProgress);
    connect(m_Engine, &FileTransferEngine::placesLoaded, this, [this](bool remote, const QList<TransferPlace>& places, const QString& error) {
        auto& pane = m_Panes[remote];
        pane.places = places;
        pane.placesError = error;
    });
    connect(m_Engine, &FileTransferEngine::directoryCreated, this, [this](bool remote, const QString& path, const QString& error) {
        auto& pane = m_Panes[remote];
        if (pane.requested != path) return;
        if (!error.isEmpty()) pane.message->setText(error);
        else m_Engine->list(remote, path);
    });
    m_SpeedTimer = new QTimer(this);
    m_SpeedTimer->setInterval(250);
    connect(m_SpeedTimer, &QTimer::timeout, this, &FileTransferWindow::refreshSpeed);
    connect(m_Engine, &FileTransferEngine::networkProgress, this, [this](int id, qint64 attemptBytes) {
        if (!m_Running.contains(id)) return;
        if (m_SpeedJob != id) {
            m_SpeedJob = id;
            m_Speed.reset();
            m_Speed.sample(0);
            m_SpeedTimer->start();
        }
        m_AttemptBytes = attemptBytes;
    });
    connect(m_Engine, &FileTransferEngine::interrupted, this, [this](int id) {
        if (auto* item = m_Running.value(id)) m_Tasks[item].retryable = true;
    });
    connect(m_Resume, &QPushButton::clicked, this, [this] {
        startTasks(m_Queue->selectedItems());
        updateActions();
    });
    connect(m_Cancel, &QPushButton::clicked, this, [this] {
        for (auto* item : m_Queue->selectedItems()) cancelTask(item);
        updateActions();
    });
    connect(m_Clear, &QPushButton::clicked, this, [this] {
        for (int i = m_Queue->topLevelItemCount() - 1; i >= 0; --i) {
            auto* item = m_Queue->topLevelItem(i);
            if (m_Tasks.value(item).state == State::Finished) {
                m_Tasks.remove(item);
                delete item;
            }
        }
        updateActions();
    });
    connect(m_Queue, &QTreeWidget::itemSelectionChanged, this, &FileTransferWindow::updateActions);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &FileTransferWindow::updateActions);
    m_FileIcons = new TransferFileIcons(this, [this] {
        for (auto& pane : m_Panes) pane.files->viewport()->update();
        m_Queue->viewport()->update();
    });
    if (!certificate.isNull()) {
        // A host's display name or IP may change between connections.
        const auto peer = QCryptographicHash::hash(certificate.toDer() + '\0' + hostId.toUtf8(), QCryptographicHash::Sha256).toHex();
        m_QueueSettings = QStringLiteral("fileTransfer/pending/v1/") + QString::fromLatin1(peer) + '/';
        m_LockDirectory = QDir::temp().filePath(QStringLiteral("moonlight-file-queue/") + QString::fromLatin1(peer));
        QDir().mkpath(m_LockDirectory);
        restoreTasks();
    }
    m_SaveTimer.start();
    m_Engine->loadPlaces(false);
    if (m_Available) m_Engine->loadPlaces(true);
    restoreDirectory(false, QDir::homePath());
    if (m_Available)
        restoreDirectory(true, {});
    else {
        m_Panes[1].message->setText(tr("This host did not grant file access. Check the Sunshine version and file transfer permission."));
        m_Panes[1].address->setEnabled(false);
    }
    updateActions();
}

QWidget* FileTransferWindow::makePane(bool remote, const QString& title)
{
    auto& p = m_Panes[remote];
    p.transferRefresh.setSingleShot(true);
    connect(&p.transferRefresh, &QTimer::timeout, this, [this, remote] {
        m_Engine->list(remote, m_Panes[remote].requested);
    });
    auto* box = new QFrame;
    box->setObjectName("transferPanel");
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
    auto* bar = new QHBoxLayout;
    auto* label = heading(title);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    label->setToolTip(title);
    p.send = new QPushButton(remote ? tr("← Download") : tr("Upload →"));
    p.send->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    auto* actionsButton = new QToolButton;
    actionsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    actionsButton->setObjectName("transferMenu");
    actionsButton->setText(tr("File actions"));
    actionsButton->setPopupMode(QToolButton::InstantPopup);
    auto* actionsMenu = new QMenu(actionsButton);
    actionsButton->setMenu(actionsMenu);

    if (remote) bar->addWidget(p.send);
    bar->addWidget(label, 1);
    bar->addWidget(actionsButton);
    if (!remote) bar->addWidget(p.send);
    layout->addLayout(bar);

    auto* nav = new QHBoxLayout;
    nav->setSpacing(3);
    p.back = tool(nav, QStringLiteral("go-previous"), QStyle::SP_ArrowBack, tr("Back"));
    p.up = tool(nav, QStringLiteral("go-up"), QStyle::SP_ArrowUp, tr("Parent folder"));
    p.refresh = tool(nav, QStringLiteral("view-refresh"), QStyle::SP_BrowserReload, tr("Refresh"));
    p.newFolder = tool(nav, QStringLiteral("folder-new"), QStyle::SP_FileDialogNewFolder, tr("New folder"));
    auto* addressBox = new QFrame;
    addressBox->setObjectName("transferAddress");
    auto* addressBar = new QHBoxLayout(addressBox);
    addressBar->setContentsMargins(5, 1, 2, 1);
    addressBar->setSpacing(0);
    p.address = new QLineEdit;
    p.address->setFrame(false);
    p.address->setClearButtonEnabled(true);
    p.address->setPlaceholderText(tr("This PC"));
    p.address->setAccessibleName(tr("Folder path"));
    addressBar->addWidget(p.address, 1);
    p.locations = tool(addressBar, QStringLiteral("go-down"), QStyle::SP_ArrowDown, tr("Locations and bookmarks"));
    p.locations->setAutoRaise(true);
    auto* bookmarkButton = new QToolButton;
    bookmarkButton->setObjectName("transferTool");
    bookmarkButton->setAutoRaise(true);
    QFont starFont(QStringLiteral("Segoe UI Symbol"));
    starFont.setPixelSize(style()->pixelMetric(QStyle::PM_SmallIconSize));
    bookmarkButton->setFont(starFont);
    p.bookmark = new QAction(bookmarkButton);
    p.bookmark->setCheckable(true);
    bookmarkButton->setDefaultAction(p.bookmark);
    const auto showBookmarkState = [action = p.bookmark, bookmarkButton](bool saved) {
        const auto label = saved ? tr("Remove current bookmark") : tr("Bookmark current folder");
        action->setText(label);
        action->setIconText(saved ? QStringLiteral("★") : QStringLiteral("☆"));
        bookmarkButton->setAccessibleName(label);
    };
    showBookmarkState(false);
    connect(p.bookmark, &QAction::toggled, bookmarkButton, showBookmarkState);
    addressBar->addWidget(bookmarkButton);
    nav->addWidget(addressBox, 1);
    for (auto* button : {p.back, p.up, p.refresh, p.newFolder}) {
        button->setAutoRaise(true);
        button->setMinimumSize(28, 30);
    }
    layout->addLayout(nav);
    auto* files = new FileList(remote);
    p.files = files;
    files->selectedFiles = [this, remote] { return selection(remote); };
    files->setToolTip(tr("Drag files or folders to the other PC. Drop on a folder to copy into it."));
    files->destinationAt = [this, remote, files](QPoint point) -> QString {
        const auto& pane = m_Panes[remote];
        if (!m_Available || !pane.ready || !files->viewport()->rect().contains(point)) return {};
        if (auto* item = files->itemAt(point)) {
            const auto entry = item->data(0, Qt::UserRole).value<TransferEntry>();
            if (entry.directory) return entry.path;
        }
        return pane.path;
    };
    files->upload = [this](const auto& entries, const QString& folder) { transferTo(false, entries, folder); };
    files->downloadAt = [this](const auto& entries, QPoint global) {
        auto* local = static_cast<FileList*>(m_Panes[0].files);
        const auto point = local->viewport()->mapFromGlobal(global);
        const auto copy = [this, entries](const QString& folder) {
            if (!folder.isEmpty()) transferTo(true, entries, folder);
            else m_Panes[1].message->setText(tr("Drop the selection in the local pane, in File Explorer, or on the desktop."));
        };
        if (QApplication::widgetAt(global) == local->viewport()) {
            copy(local->destinationAt(point));
        } else {
            const auto pos = GetMessagePos();
            const POINT physical{short(LOWORD(pos)), short(HIWORD(pos))};
            m_Engine->localDropFolder(QPoint(physical.x, physical.y), reinterpret_cast<quintptr>(WindowFromPhysicalPoint(physical)), copy);
        }
    };
    FileTransferStyle::configureList(p.files, {QString(), tr("Name"), tr("Modified"), tr("Type"), tr("Size")});
    p.files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    p.files->header()->setSortIndicator(1, Qt::AscendingOrder);
    p.files->setSortingEnabled(true);
    p.files->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    p.files->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    p.files->setColumnWidth(0, 36);
    p.files->setColumnWidth(2, 129);
    p.files->setColumnWidth(3, 65);
    p.files->setColumnWidth(4, 78);
    // Cover the whole selection header so a click beside the checkbox cannot sort by it.
    auto* header = p.files->header();
    auto* selectionHeader = new QWidget(header->viewport());
    selectionHeader->setAttribute(Qt::WA_NoMousePropagation);
    auto* selectionLayout = new QHBoxLayout(selectionHeader);
    selectionLayout->setContentsMargins(0, 0, 0, 0);
    p.selectAll = new QCheckBox;
    // The native Windows 11 checkbox paints the system accent even on a custom light surface.
    if (!FileTransferStyle::highContrastEnabled()) {
        if (auto* indicatorStyle = QStyleFactory::create(QStringLiteral("Fusion"))) {
            indicatorStyle->setParent(p.selectAll);
            p.selectAll->setStyle(indicatorStyle);
            p.selectAll->setPalette(palette());
        }
    }
    p.files->setItemDelegateForColumn(0, new SelectionColumn(p.selectAll, p.files));
    selectionLayout->addWidget(p.selectAll, 0, Qt::AlignCenter);
    p.selectAll->setAccessibleName(tr("Select all"));
    p.selectAll->setToolTip(tr("Select all"));
    auto positionCheck = [header, selectionHeader] {
        selectionHeader->setGeometry(header->sectionViewportPosition(0), 0, header->sectionSize(0), header->height());
    };
    connect(header, &QHeaderView::geometriesChanged, this, positionCheck);
    connect(p.files->horizontalScrollBar(), &QScrollBar::valueChanged, this, positionCheck);
    positionCheck();
    connect(p.selectAll, &QCheckBox::clicked, this, [this, remote] {
        auto* files = m_Panes[remote].files;
        if (files->selectedItems().size() == files->topLevelItemCount()) files->clearSelection();
        else files->selectAll();
    });
    layout->addWidget(p.files, 1);
    p.message = new QLabel;
    p.message->setTextFormat(Qt::PlainText);
    p.message->setWordWrap(true);
    layout->addWidget(p.message);

    p.open = new QAction(tr("Open"), p.files);
    p.remove = new QAction(tr("Delete…"), p.files);
    p.remove->setShortcut(QKeySequence::Delete);
    p.copy = new QAction(tr("Copy"), p.files);
    p.copy->setShortcut(QKeySequence::Copy);
    p.paste = new QAction(tr("Paste here"), p.files);
    p.paste->setShortcut(QKeySequence::Paste);
    for (auto* action : {p.open, p.copy, p.paste, p.remove}) {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        p.files->addAction(action);
        actionsMenu->addAction(action);
    }
    p.files->setContextMenuPolicy(Qt::ActionsContextMenu);
    connect(p.open, &QAction::triggered, this, [this, remote] {
        const auto items = m_Panes[remote].files->selectedItems();
        if (items.size() == 1) openEntry(remote, items.first()->data(0, Qt::UserRole).value<TransferEntry>());
    });
    connect(p.remove, &QAction::triggered, this, [this, remote] {
        manageSelection(FileAction::Delete, remote, selection(remote));
    });
    connect(p.copy, &QAction::triggered, this, [this, remote] { copySelection(remote); });
    connect(p.paste, &QAction::triggered, this, [this, remote] { pasteSelection(remote); });
    connect(p.send, &QPushButton::clicked, this, [this, remote] { transfer(remote, selection(remote)); });
    connect(p.back, &QToolButton::clicked, this, [this, remote] {
        auto& pane = m_Panes[remote];
        if (!pane.ready && !pane.history.isEmpty())
            navigate(remote, pane.history.last());
        else if (pane.history.size() > 1)
            navigate(remote, pane.history.at(pane.history.size() - 2));
    });
    connect(p.up, &QToolButton::clicked, this, [this, remote] {
        QDir dir(m_Panes[remote].path);
        navigate(remote, dir.isRoot() ? QString() : QDir::cleanPath(dir.filePath("..")));
    });
    connect(p.address, &QLineEdit::returnPressed, this, [this, remote] {
        navigate(remote, m_Panes[remote].address->text());
    });
    connect(p.refresh, &QToolButton::clicked, this, [this, remote] {
        navigate(remote, m_Panes[remote].requested);
        m_Engine->loadPlaces(remote);
    });
    connect(p.locations, &QToolButton::clicked, this, [this, remote] { showLocations(remote); });
    connect(p.bookmark, &QAction::triggered, this, [this, remote](bool save) {
        const auto& pane = m_Panes[remote];
        QSettings settings;
        auto bookmarks = settings.value(pane.bookmarksKey).toStringList();
        if (save) {
            if (!bookmarks.contains(pane.path)) bookmarks.append(pane.path);
        } else {
            bookmarks.removeAll(pane.path);
        }
        settings.setValue(pane.bookmarksKey, bookmarks);
    });
    connect(p.newFolder, &QToolButton::clicked, this, [this, remote] {
        const auto parent = m_Panes[remote].path;
        bool accepted = false;
        const auto name = QInputDialog::getText(this, tr("New folder"), tr("Folder name"), QLineEdit::Normal, {}, &accepted);
        if (accepted && !name.isEmpty()) m_Engine->makeDirectory(remote, parent, name);
    });
    connect(p.files, &QTreeWidget::itemSelectionChanged, this, &FileTransferWindow::updateActions);
    connect(p.files, &QTreeWidget::itemActivated, this, [this, remote](QTreeWidgetItem* item) {
        openEntry(remote, item->data(0, Qt::UserRole).value<TransferEntry>());
    });
    return box;
}

void FileTransferWindow::restoreDirectory(bool remote, const QString& defaultPath)
{
    const auto saved = QSettings().value(m_Panes[remote].lastDirectoryKey, defaultPath).toString();
    if (!saved.isEmpty() && QDir::isRelativePath(saved))
        navigate(remote, defaultPath);
    else
        navigate(remote, saved, defaultPath);
}

void FileTransferWindow::navigate(bool remote, QString path, std::optional<QString> restoreFallback)
{
    auto& p = m_Panes[remote];
    path = QDir::fromNativeSeparators(path);
    if (!path.isEmpty() && QDir::isRelativePath(path)) {
        if (p.path.isEmpty()) {
            p.message->setText(tr("Enter an absolute folder path."));
            return;
        }
        path = QDir(p.path).filePath(path);
    }
    p.requested = path.isEmpty() ? QString() : QDir::cleanPath(path);
    p.transferRefresh.stop();
    if (remote) {
        m_RemoteRetryTimer.stop();
        m_RemoteRetryDelay = 0;
    }
    p.restoreFallback = std::move(restoreFallback);
    p.ready = false;
    p.address->setText(nativePath(p.requested));
    p.files->clear();
    p.message->setText(tr("Loading…"));
    updateActions();
    m_Engine->list(remote, p.requested);
}

void FileTransferWindow::showDirectory(bool remote, QString path, const QList<TransferEntry>& entries, QString error,
                                       bool waitingForDesktop)
{
    auto& p = m_Panes[remote];
    if (p.requested != QDir::fromNativeSeparators(path))
        return;
    if (remote && waitingForDesktop) {
        p.ready = false;
        p.files->clear();
        p.message->setText(tr("The file list will refresh when the host desktop is available. If you changed users, reconnect."));
        m_RemoteRetryDelay = m_RemoteRetryDelay ? qMin(30000, 2 * m_RemoteRetryDelay) : 1000;
        if (isVisible() && !isMinimized()) m_RemoteRetryTimer.start(m_RemoteRetryDelay);
        updateActions();
        return;
    }
    if (remote && m_RemoteRetryDelay) {
        m_RemoteRetryTimer.stop();
        m_RemoteRetryDelay = 0;
        if (error.isEmpty()) m_Engine->loadPlaces(true);
    }
    const auto fallback = std::exchange(p.restoreFallback, std::nullopt);
    if (!error.isEmpty() && fallback && p.requested != *fallback) {
        navigate(remote, *fallback);
        return;
    }
    // Keep the current selection and scroll position when a background transfer
    // refreshes this folder while the user is preparing another copy.
    const QSignalBlocker blocker(p.files);
    QSet<QString> selected;
    const int scroll = p.files->verticalScrollBar()->value();
    for (const auto* item : p.files->selectedItems())
        selected.insert(item->text(1));
    p.files->clear();
    p.ready = error.isEmpty();
    if (!p.ready) {
        p.message->setText(error);
        updateActions();
        return;
    }
    if (!fallback && (p.history.isEmpty() || p.path != p.requested))
        QSettings().setValue(p.lastDirectoryKey, p.requested);
    p.path = p.requested;
    p.bookmark->setChecked(!p.path.isEmpty() && QSettings().value(p.bookmarksKey).toStringList().contains(p.path));
    if (p.history.size() >= 2 && p.history.at(p.history.size() - 2) == p.path)
        p.history.removeLast();
    else if (p.history.isEmpty() || p.history.last() != p.path)
        p.history.append(p.path);
    const auto folder = style()->standardIcon(p.path.isEmpty() ? QStyle::SP_DriveHDIcon : QStyle::SP_DirIcon);
    const auto file = style()->standardIcon(QStyle::SP_FileIcon);
    p.files->setSortingEnabled(false);
    for (const auto& entry : entries) {
        // Sort local files and folders together by the selected column.
        auto* item = new TransferListItem(1, entry, remote, m_FileIcons, /*pinFoldersFirst=*/remote);
        item->setText(1, entry.name);
        item->setToolTip(1, nativePath(entry.path));
        item->setIcon(1, entry.directory ? folder : file);
        const auto suffix = QFileInfo(entry.name).suffix();
        item->setText(3, p.path.isEmpty() ? tr("Drive") : entry.directory ? tr("Folder") : suffix.isEmpty() ? tr("File") : suffix);
        if (!p.path.isEmpty() && entry.modified.isValid())
            setNumericCell(item, 2, entry.modified.toMSecsSinceEpoch(),
                           entry.modified.toLocalTime().toString(QStringLiteral("yyyy/MM/dd HH:mm")));
        if (!entry.directory)
            setNumericCell(item, 4, entry.size, locale().formattedDataSize(entry.size));
        item->setData(0, Qt::UserRole, QVariant::fromValue(entry));
        p.files->addTopLevelItem(item);
        if (selected.contains(entry.name)) item->setSelected(true);
    }
    p.files->setSortingEnabled(true);
    p.files->verticalScrollBar()->setValue(scroll);
    p.message->setText(entries.isEmpty() ? tr("Empty folder") : tr("%1 items").arg(entries.size()));
    updateActions();
}

QList<TransferEntry> FileTransferWindow::selection(bool remote) const
{
    QList<TransferEntry> entries;
    const auto& p = m_Panes[remote];
    if (p.ready && !p.path.isEmpty()) {
        for (const auto* item : p.files->selectedItems())
            entries.append(item->data(0, Qt::UserRole).value<TransferEntry>());
    }
    return entries;
}

void FileTransferWindow::updateActions()
{
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    const bool ours = mime && mime->data(clipboardType) == m_ClipboardId;
    for (int side = 0; side < 2; ++side) {
        auto& p = m_Panes[side];
        const bool available = !side || m_Available;
        const bool destination = p.ready && !p.path.isEmpty();
        const auto items = p.files->selectedItems();
        const bool hasSelection = destination && !items.isEmpty();
        p.back->setEnabled(available && (p.history.size() > 1 || (!p.ready && !p.history.isEmpty())));
        p.up->setEnabled(destination);
        p.refresh->setEnabled(available);
        p.locations->setEnabled(available);
        p.bookmark->setEnabled(destination);
        if (!destination) p.bookmark->setChecked(false);
        p.newFolder->setEnabled(destination);
        const int count = p.files->topLevelItemCount();
        const int selected = items.size();
        p.selectAll->setEnabled(p.ready && count > 0);
        p.selectAll->setCheckState(!selected ? Qt::Unchecked : selected == count ? Qt::Checked : Qt::PartiallyChecked);
        const bool singleFile = selected == 1 && !items.first()->data(0, Qt::UserRole).value<TransferEntry>().directory;
        p.open->setText(side && singleFile ? tr("Open temporary copy") : tr("Open"));
        p.open->setEnabled(p.ready && selected == 1 && (!p.operating || !singleFile));
        p.remove->setEnabled(!p.operating && hasSelection);
        p.copy->setEnabled(hasSelection);
        const bool canPaste = ours ? !m_Copied.isEmpty() : mime && mime->hasUrls();
        p.paste->setEnabled(destination && !p.operating && canPaste);
        p.send->setEnabled(hasSelection && m_Panes[!side].ready && !m_Panes[!side].path.isEmpty());
    }
    bool canCancel = false, canResume = false;
    for (auto* item : m_Queue->selectedItems()) {
        const auto state = m_Tasks.value(item).state;
        canCancel |= state == State::Active || state == State::Interrupted;
        canResume |= state == State::Interrupted;
    }
    m_Resume->setEnabled(canResume && m_Available);
    m_Cancel->setEnabled(canCancel);
    m_Clear->setEnabled(std::any_of(m_Tasks.cbegin(), m_Tasks.cend(), [](const Task& t) { return t.state == State::Finished; }));
}

void FileTransferWindow::showLocations(bool remote)
{
    auto& p = m_Panes[remote];
    const auto bookmarks = QSettings().value(p.bookmarksKey).toStringList();
    QMenu menu(this);
    menu.setMinimumWidth(p.address->parentWidget()->width());
    auto addPath = [&](QString label, const QString& path, QStyle::StandardPixmap icon) {
        auto* action = menu.addAction(style()->standardIcon(icon), label.replace("&", "&&"));
        action->setData(path);
        if (p.ready && path.compare(p.path, Qt::CaseInsensitive) == 0) {
            action->setCheckable(true);
            action->setChecked(true);
        }
    };
    addPath(tr("This PC"), {}, QStyle::SP_ComputerIcon);
    for (const auto& location : p.places)
        if (location.kind == "drive")
            addPath(location.name.isEmpty() ? nativePath(location.path) : location.name, location.path, QStyle::SP_DriveHDIcon);
    const struct { const char* kind; QString label; QStyle::StandardPixmap icon; } known[] = {
        {"documents", tr("Documents"), QStyle::SP_FileDialogDetailedView}, {"desktop", tr("Desktop"), QStyle::SP_DesktopIcon},
        {"pictures", tr("Pictures"), QStyle::SP_FileDialogContentsView}, {"videos", tr("Videos"), QStyle::SP_MediaPlay},
        {"music", tr("Music"), QStyle::SP_MediaVolume}, {"downloads", tr("Downloads"), QStyle::SP_ArrowDown}
    };
    bool first = true;
    for (const auto& folder : known) {
        for (const auto& place : p.places) {
            if (place.kind != QLatin1String(folder.kind)) continue;
            if (first) { menu.addSeparator(); first = false; }
            addPath(folder.label, place.path, folder.icon);
        }
    }
    if (p.places.isEmpty() && !p.placesError.isEmpty())
        menu.addAction(tr("Locations unavailable"))->setEnabled(false);
    menu.addSeparator();
    if (!p.path.isEmpty() && !bookmarks.contains(p.path))
        addPath(nativePath(p.path), p.path, QStyle::SP_DirOpenIcon);
    for (const auto& path : bookmarks)
        addPath(nativePath(path), path, QStyle::SP_DirIcon);
    p.bookmark->setChecked(p.bookmark->isEnabled() && bookmarks.contains(p.path));
    menu.addAction(p.bookmark);
    auto* anchor = p.address->parentWidget();
    auto* chosen = menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
    if (chosen && chosen != p.bookmark) navigate(remote, chosen->data().toString());
}

void FileTransferWindow::openLocalFile(const QString& path)
{
    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink() || info.isJunction() ||
        !QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        QMessageBox::warning(this, tr("Open"), tr("Could not open %1.").arg(nativePath(path)));
}

void FileTransferWindow::openEntry(bool remote, const TransferEntry& entry)
{
    const auto& pane = m_Panes[remote];
    if (!pane.ready || (pane.operating && !entry.directory)) return;
    if (entry.directory) navigate(remote, entry.path);
    else if (remote) manageSelection(FileAction::DownloadToOpen, true, {entry});
    else openLocalFile(entry.path);
}

void FileTransferWindow::manageSelection(FileAction action, bool remote, const QList<TransferEntry>& entries)
{
    auto& p = m_Panes[remote];
    if (!p.ready || p.path.isEmpty() || p.operating || entries.isEmpty()) return;
    const auto viewPath = p.path;
    QString folder = viewPath;
    QString openPath;
    if (action == FileAction::Delete) {
        QStringList paths;
        for (const auto& entry : entries) paths << nativePath(entry.path);
        QMessageBox confirm(QMessageBox::Question, tr("Delete selected items"), {}, QMessageBox::Yes | QMessageBox::Cancel, this);
        confirm.setTextFormat(Qt::PlainText);
        confirm.setText(remote ? tr("Permanently delete the selected items and all folder contents on the remote PC?") :
                                 tr("Move the selected items and all folder contents to the Recycle Bin on this PC?"));
        confirm.setInformativeText(paths.size() == 1 ? paths.first() : tr("%1 items in %2").arg(paths.size()).arg(nativePath(folder)));
        if (paths.size() > 1) confirm.setDetailedText(paths.join('\n'));
        confirm.setDefaultButton(QMessageBox::Cancel);
        if (confirm.exec() != QMessageBox::Yes) return;
        p.message->setText(tr("Deleting files…"));
    } else if (action == FileAction::DownloadToOpen) {
        if (!m_OpenCopies) m_OpenCopies = std::make_unique<QTemporaryDir>();
        folder = m_OpenCopies->filePath(QUuid::createUuid().toString(QUuid::Id128));
        if (!m_OpenCopies->isValid() || !QDir().mkdir(folder)) {
            p.message->setText(tr("Could not create a temporary folder."));
            return;
        }
        openPath = QDir(folder).filePath(entries.first().name);
        p.message->setText(tr("Downloading %1 to open…").arg(entries.first().name));
    } else {
        p.message->setText(tr("Copying files…"));
    }
    p.operating = true;
    updateActions();
    m_Engine->manageFiles(action, remote, entries, folder, [this, remote, viewPath, openPath](const QString& error) {
        auto& pane = m_Panes[remote];
        pane.operating = false;
        if (pane.requested == viewPath) {
            if (openPath.isEmpty()) pane.ready = false;
            m_Engine->list(remote, viewPath);
        }
        updateActions();
        if (isVisible()) {
            if (!error.isEmpty()) QMessageBox::warning(this, tr("File transfer"), error);
            else if (!openPath.isEmpty()) openLocalFile(openPath);
        }
        closeWhenIdle();
    });
}

void FileTransferWindow::copySelection(bool remote)
{
    m_Copied = selection(remote);
    if (m_Copied.isEmpty())
        return;
    m_CopiedRemote = remote;
    auto* mime = new QMimeData;
    if (!remote) {
        QList<QUrl> urls;
        for (const auto& entry : m_Copied) urls << QUrl::fromLocalFile(entry.path);
        mime->setUrls(urls);
    }
    mime->setData(clipboardType, m_ClipboardId);
    QApplication::clipboard()->setMimeData(mime);
}

void FileTransferWindow::pasteSelection(bool remote)
{
    const auto* mime = QApplication::clipboard()->mimeData();
    if (!mime)
        return;
    if (mime->data(clipboardType) == m_ClipboardId) {
        if (m_CopiedRemote != remote)
            transfer(!remote, m_Copied);
        else
            manageSelection(FileAction::Copy, remote, m_Copied);
    }
    else if (mime->hasUrls()) {
        QList<TransferEntry> entries;
        for (const auto& url : mime->urls()) {
            if (!url.isLocalFile())
                continue;
            QFileInfo info(url.toLocalFile());
            if (!info.exists() || info.isRoot() || info.isSymLink() || info.isJunction())
                continue;
            TransferEntry entry;
            entry.name = info.fileName();
            entry.path = info.absoluteFilePath();
            entry.directory = info.isDir();
            entry.size = info.size();
            entry.modified = info.lastModified();
            entries.append(entry);
        }
        if (remote) transfer(false, entries);
        else manageSelection(FileAction::Copy, false, entries);
    }
}

FileTransferWindow::~FileTransferWindow()
{
    // Keep the task lock until the worker has stopped and saved its partial file.
    delete m_Engine;
}

QTreeWidgetItem* FileTransferWindow::addTask(bool fromRemote, const TransferEntry& entry,
                                             const QString& destination, const QString& key, qint64 bytes)
{
    auto* item = new TransferListItem(0, entry, fromRemote, m_FileIcons);
    m_Tasks.insert(item, Task{entry, destination, key, fromRemote, bytes});
    item->setText(0, entry.name);
    item->setIcon(0, style()->standardIcon(entry.directory ? QStyle::SP_DirIcon : QStyle::SP_FileIcon));
    item->setText(1, tr("Interrupted"));
    setNumericCell(item, 2, bytes, byteCounts(locale(), bytes, entry.directory ? -1 : entry.size));
    const auto source = fromRemote ? tr("Remote · %1") : tr("Local · %1");
    const auto target = fromRemote ? tr("Local · %1") : tr("Remote · %1");
    item->setText(4, source.arg(nativePath(entry.path)));
    item->setText(5, target.arg(nativePath(destination)));
    item->setToolTip(4, item->text(4));
    item->setToolTip(5, item->text(5));
    m_Queue->addTopLevelItem(item);
    return item;
}

std::shared_ptr<QLockFile> FileTransferWindow::claimTask(const QString& key)
{
    auto lock = std::make_shared<QLockFile>(QDir(m_LockDirectory).filePath(key + ".lock"));
    lock->setStaleLockTime(0); // A healthy transfer can run for hours.
    return lock->tryLock() ? lock : nullptr;
}

void FileTransferWindow::restoreTasks()
{
    QSettings settings;
    settings.beginGroup(m_QueueSettings);
    for (const auto& key : settings.childKeys()) {
        // The device window and the in-stream file browser may run in separate processes.
        const auto lock = claimTask(key);
        if (!lock) continue;
        settings.sync();
        const auto saved = settings.value(key).toMap();
        TransferEntry entry{saved.value("name").toString(), saved.value("path").toString(),
                            saved.value("dir").toBool(), saved.value("size").toLongLong(),
                            saved.value("modified").toDateTime()};
        const auto destination = saved.value("destination").toString();
        const bool fromRemote = saved.value("fromRemote").toBool();
        if (entry.name.isEmpty() || entry.path.isEmpty() || destination.isEmpty() ||
                taskKey(fromRemote, entry.path, destination) != key) continue;
        auto* item = addTask(fromRemote, entry, destination, key, saved.value("bytes").toLongLong());
        m_Tasks[item].remembered = true;
    }
}

void FileTransferWindow::saveTask(QTreeWidgetItem* item)
{
    if (m_QueueSettings.isEmpty()) return;
    auto& t = m_Tasks[item];
    t.remembered = true;
    QSettings().setValue(m_QueueSettings + t.key, QVariantMap{
        {"name", t.entry.name}, {"path", t.entry.path}, {"dir", t.entry.directory}, {"size", t.entry.size},
        {"modified", t.entry.modified}, {"destination", t.destination}, {"fromRemote", t.fromRemote}, {"bytes", t.bytes}
    });
}

bool FileTransferWindow::takeTask(QTreeWidgetItem* item)
{
    auto& t = m_Tasks[item];
    if (m_QueueSettings.isEmpty()) return true;
    t.lock = claimTask(t.key);
    if (!t.lock) {
        t.remembered = true;
        item->setText(1, tr("Transfer is busy in another window"));
        return false;
    }
    if (t.remembered && !QSettings().contains(m_QueueSettings + t.key)) {
        t.lock.reset();
        t.state = State::Finished;
        item->setText(1, tr("Transfer ended in another window"));
        return false;
    }
    return true;
}

void FileTransferWindow::startTasks(const QList<QTreeWidgetItem*>& items)
{
    if (!m_Available) return;
    QList<QTreeWidgetItem*> ready;
    for (auto* item : items) {
        auto& task = m_Tasks[item];
        if (task.state != State::Interrupted || !takeTask(item)) continue;
        task.state = State::Active;
        task.retryable = false;
        saveTask(item);
        item->setText(1, tr("Queued"));
        item->setToolTip(1, {});
        setNumericCell(item, 3, {}, {});
        ready.append(item);
    }
    // Only combine tasks with the same direction and destination, in queue order.
    for (qsizetype first = 0; first < ready.size();) {
        const auto& batch = m_Tasks[ready[first]];
        QList<TransferEntry> entries;
        qsizetype next = first;
        for (; next < ready.size(); ++next) {
            const auto& task = m_Tasks[ready[next]];
            if (task.fromRemote != batch.fromRemote || task.destination != batch.destination) break;
            entries.append(task.entry);
        }
        const auto ids = m_Engine->enqueue(batch.fromRemote, entries, batch.destination);
        for (qsizetype i = 0; i < ids.size(); ++i) {
            auto* item = ready[first + i];
            m_Tasks[item].id = ids[i];
            m_Running.insert(ids[i], item);
        }
        first = next;
    }
}

void FileTransferWindow::cancelTask(QTreeWidgetItem* item)
{
    auto& t = m_Tasks[item];
    if (t.state == State::Active) {
        t.state = State::Cancelling;
        item->setText(1, tr("Cancelling…"));
        if (!m_QueueSettings.isEmpty()) QSettings().remove(m_QueueSettings + t.key);
        m_Engine->cancel(t.id);
    } else if (t.state == State::Interrupted) {
        if (!takeTask(item)) return;
        if (!m_QueueSettings.isEmpty()) QSettings().remove(m_QueueSettings + t.key);
        t.state = State::Cancelling;
        item->setText(1, tr("Cancelling…"));
        item->setToolTip(1, {});
        m_Engine->discard(t.fromRemote, t.entry, t.destination, [this, item] {
            auto& task = m_Tasks[item];
            task.state = State::Finished;
            task.lock.reset();
            item->setText(1, tr("Cancelled"));
            updateActions();
        });
    }
}

void FileTransferWindow::transfer(bool fromRemote, const QList<TransferEntry>& entries)
{
    const auto& destination = m_Panes[!fromRemote];
    if (!destination.ready || destination.path.isEmpty()) return;
    transferTo(fromRemote, entries, destination.path);
}

void FileTransferWindow::transferTo(bool fromRemote, const QList<TransferEntry>& entries, const QString& destination)
{
    if (!m_Available || !QDir::isAbsolutePath(destination)) return;
    QHash<QString, QTreeWidgetItem*> unfinished;
    for (auto it = m_Tasks.cbegin(); it != m_Tasks.cend(); ++it) {
        if (it->state != State::Finished) unfinished.insert(it->key, it.key());
    }
    QList<QTreeWidgetItem*> items;
    for (const auto& entry : entries) {
        const auto key = taskKey(fromRemote, entry.path, destination);
        auto* item = unfinished.value(key);
        if (item) {
            if (m_Tasks[item].state == State::Interrupted) m_Tasks[item].entry = entry;
            m_Queue->setCurrentItem(item);
            m_Queue->scrollToItem(item);
        } else {
            item = addTask(fromRemote, entry, destination, key, 0);
            unfinished.insert(key, item);
        }
        items.append(item);
    }
    startTasks(items);
    updateActions();
}

bool FileTransferWindow::hasPendingWork() const
{
    return !m_Running.isEmpty() || m_Panes[0].operating || m_Panes[1].operating;
}

void FileTransferWindow::closeWhenIdle()
{
    if (m_CloseAfterTransfers && !isVisible() && !hasPendingWork())
        QTimer::singleShot(0, this, [this] { if (!isVisible() && !hasPendingWork()) close(); });
}

void FileTransferWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_RemoteRetryDelay) m_RemoteRetryTimer.start(0);
}

void FileTransferWindow::closeEvent(QCloseEvent* event)
{
    if (m_Standalone && hasPendingWork()) {
        // Closing the browser lets queued transfers finish in the background.
        m_CloseAfterTransfers = true;
        hide();
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
    if (event->isAccepted()) emit closed();
}

void FileTransferWindow::refreshSpeed()
{
    auto* item = m_Running.value(m_SpeedJob);
    if (!item) return;
    const auto rate = m_Speed.sample(m_AttemptBytes);
    if (rate < 0)
        setNumericCell(item, 3, {}, {});
    else
        setNumericCell(item, 3, rate, tr("%1/s").arg(locale().formattedDataSize(rate)));
}

void FileTransferWindow::showProgress(int id, qint64 bytes, qint64 total, const QString& status, bool finished)
{
    auto* item = m_Running.value(id);
    if (!item) return;
    auto& t = m_Tasks[item];
    t.bytes = bytes;
    item->setText(1, finished && t.retryable && t.state != State::Cancelling ? tr("Interrupted") : status);
    item->setToolTip(1, status);
    setNumericCell(item, 2, bytes, byteCounts(locale(), bytes, total));
    item->setToolTip(2, item->text(2));
    if (!finished) {
        if (m_SaveTimer.elapsed() >= 2000 && t.state == State::Active) {
            saveTask(item);
            m_SaveTimer.restart();
        }
        return;
    }
    if (m_SpeedJob == id) {
        m_SpeedTimer->stop();
        m_SpeedJob = 0;
        setNumericCell(item, 3, {}, {});
    }
    m_Running.remove(id);
    t.id = 0;
    if (t.retryable && t.state != State::Cancelling) {
        t.state = State::Interrupted;
        saveTask(item);
    } else {
        t.state = State::Finished;
        if (!m_QueueSettings.isEmpty()) QSettings().remove(m_QueueSettings + t.key);
    }
    t.lock.reset();
    const bool remoteDestination = !t.fromRemote;
    auto& pane = m_Panes[remoteDestination];
    if (pane.requested == t.destination) {
        // Refresh once after a burst of completions, or immediately when the queue drains.
        pane.transferRefresh.start(m_Running.isEmpty() ? 0 : 750);
    }
    updateActions();
    closeWhenIdle();
}
