#include "filetransferwindow.h"

#include <QAction>
#include <QApplication>
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
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QHostInfo>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QToolButton>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr char clipboardType[] = "application/x-moonlight-transfer-selection";

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

QLabel* heading(const QString& text)
{
    auto* label = new QLabel(text);
    label->setTextFormat(Qt::PlainText);
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() + 2);
    font.setBold(true);
    label->setFont(font);
    return label;
}

QToolButton* tool(QHBoxLayout* layout, QStyle::StandardPixmap icon, const QString& text)
{
    auto* button = new QToolButton;
    button->setIcon(QApplication::style()->standardIcon(icon));
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
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);
        const auto* style = option.widget->style();
        QStyleOptionButton check;
        check.initFrom(option.widget);
        const QSize size(style->pixelMetric(QStyle::PM_IndicatorWidth), style->pixelMetric(QStyle::PM_IndicatorHeight));
        check.rect = QStyle::alignedRect(option.direction, Qt::AlignCenter, size, option.rect);
        check.state |= option.state.testFlag(QStyle::State_Selected) ? QStyle::State_On : QStyle::State_Off;
        style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &check, painter, option.widget);
    }
};

class FileList final : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;
protected:
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
};

void configureList(QTreeWidget* tree, const QStringList& labels)
{
    tree->setHeaderLabels(labels);
    tree->setRootIsDecorated(false);
    tree->setUniformRowHeights(true);
    tree->setAlternatingRowColors(true);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
}
}

FileTransferWindow::FileTransferWindow(QUrl host, QSslCertificate certificate, QByteArray token,
                                       QString hostName, QString hostId, QWidget* parent, bool standalone)
    : QWidget(parent, Qt::Window)
    , m_Engine(new FileTransferEngine(host, certificate, token, this, standalone))
    , m_ClipboardId(QUuid::createUuid().toRfc4122())
    , m_Available(!token.isEmpty())
    , m_Standalone(standalone)
{
    setWindowTitle(tr("File transfer"));
    resize(1160, 710);
    auto* layout = new QVBoxLayout(this);
    auto* vertical = new QSplitter(Qt::Vertical);
    auto* browsers = new QSplitter(Qt::Horizontal);
    layout->addWidget(vertical);
    vertical->addWidget(browsers);
    m_Panes[0].settingsKey = QStringLiteral("fileTransfer/bookmarks/local");
    m_Panes[1].settingsKey = QStringLiteral("fileTransfer/bookmarks/hosts/") + (hostId.isEmpty() ? host.host() : hostId);
    browsers->addWidget(makePane(false, tr("Local · %1").arg(QHostInfo::localHostName())));
    browsers->addWidget(makePane(true, tr("Remote · %1").arg(hostName)));
    browsers->setChildrenCollapsible(false);
    browsers->setSizes({550, 550});

    auto* queuePanel = new QWidget;
    auto* queueLayout = new QVBoxLayout(queuePanel);
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
    configureList(m_Queue, {tr("Name"), tr("Status"), tr("Transferred"), tr("Speed"), tr("Source"), tr("Destination")});
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
        for (auto* item : m_Queue->selectedItems()) {
            if (m_Tasks[item].state == State::Interrupted) startTask(item);
        }
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
    navigate(false, QDir::homePath());
    if (m_Available)
        navigate(true, {});
    else {
        m_Panes[1].message->setText(tr("This host did not grant file access. Check the Sunshine version and file transfer permission."));
        m_Panes[1].address->setEnabled(false);
    }
    updateActions();
}

QWidget* FileTransferWindow::makePane(bool remote, const QString& title)
{
    auto& p = m_Panes[remote];
    auto* box = new QWidget;
    auto* layout = new QVBoxLayout(box);
    auto* bar = new QHBoxLayout;
    auto* label = heading(title);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    label->setToolTip(title);
    bar->addWidget(label, 1);
    p.send = new QPushButton(remote ? tr("← Download") : tr("Upload →"));
    bar->addWidget(p.send);
    layout->addLayout(bar);

    auto* nav = new QHBoxLayout;
    nav->setSpacing(3);
    p.back = tool(nav, QStyle::SP_ArrowBack, tr("Back"));
    p.up = tool(nav, QStyle::SP_ArrowUp, tr("Parent folder"));
    p.refresh = tool(nav, QStyle::SP_BrowserReload, tr("Refresh"));
    p.newFolder = tool(nav, QStyle::SP_FileDialogNewFolder, tr("New folder"));
    auto* addressBox = new QFrame;
    addressBox->setObjectName("transferAddress");
    addressBox->setStyleSheet(QStringLiteral("QFrame#transferAddress { background: palette(base); border: 1px solid palette(midlight); border-radius: 5px; }"));
    auto* addressBar = new QHBoxLayout(addressBox);
    addressBar->setContentsMargins(5, 1, 2, 1);
    addressBar->setSpacing(0);
    p.address = new QLineEdit;
    p.address->setFrame(false);
    p.address->setClearButtonEnabled(true);
    p.address->setPlaceholderText(tr("This PC"));
    p.address->setAccessibleName(tr("Folder path"));
    addressBar->addWidget(p.address, 1);
    p.locations = tool(addressBar, QStyle::SP_ArrowDown, tr("Locations and bookmarks"));
    p.locations->setAutoRaise(true);
    nav->addWidget(addressBox, 1);
    for (auto* button : {p.back, p.up, p.refresh, p.newFolder}) {
        button->setAutoRaise(true);
        button->setMinimumSize(28, 30);
    }
    layout->addLayout(nav);
    p.files = new FileList;
    configureList(p.files, {QString(), tr("Name"), tr("Modified"), tr("Type"), tr("Size")});
    p.files->setStyleSheet(QStringLiteral("QTreeView::item { min-height: 28px; }"));
    p.files->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    p.files->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    p.files->setColumnWidth(0, 36);
    p.files->setColumnWidth(2, 129);
    p.files->setColumnWidth(3, 65);
    p.files->setColumnWidth(4, 78);
    p.files->setItemDelegateForColumn(0, new SelectionColumn(p.files));
    p.selectAll = new QCheckBox(p.files->header()->viewport());
    p.selectAll->setAccessibleName(tr("Select all"));
    p.selectAll->setToolTip(tr("Select all"));
    auto positionCheck = [&p] {
        auto* header = p.files->header();
        const QSize size = p.selectAll->sizeHint();
        p.selectAll->setGeometry(header->sectionViewportPosition(0) + (header->sectionSize(0) - size.width()) / 2,
                                 (header->height() - size.height()) / 2, size.width(), size.height());
    };
    connect(p.files->header(), &QHeaderView::geometriesChanged, this, positionCheck);
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

    p.copy = new QAction(tr("Copy"), p.files);
    p.copy->setShortcut(QKeySequence::Copy);
    p.paste = new QAction(tr("Paste here"), p.files);
    p.paste->setShortcut(QKeySequence::Paste);
    for (auto* action : {p.copy, p.paste}) {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        p.files->addAction(action);
    }
    p.files->setContextMenuPolicy(Qt::ActionsContextMenu);
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
    connect(p.newFolder, &QToolButton::clicked, this, [this, remote] {
        const auto parent = m_Panes[remote].path;
        bool accepted = false;
        const auto name = QInputDialog::getText(this, tr("New folder"), tr("Folder name"), QLineEdit::Normal, {}, &accepted);
        if (accepted && !name.isEmpty()) m_Engine->makeDirectory(remote, parent, name);
    });
    connect(p.files, &QTreeWidget::itemSelectionChanged, this, &FileTransferWindow::updateActions);
    connect(p.files, &QTreeWidget::itemActivated, this, [this, remote](QTreeWidgetItem* item) {
        const auto entry = item->data(0, Qt::UserRole).value<TransferEntry>();
        if (entry.directory)
            navigate(remote, entry.path);
    });
    return box;
}

void FileTransferWindow::navigate(bool remote, QString path)
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
    p.ready = false;
    p.address->setText(nativePath(p.requested));
    p.files->clear();
    p.message->setText(tr("Loading…"));
    updateActions();
    m_Engine->list(remote, p.requested);
}

void FileTransferWindow::showDirectory(bool remote, QString path, QList<TransferEntry> entries, QString error)
{
    auto& p = m_Panes[remote];
    if (p.requested != QDir::fromNativeSeparators(path))
        return;
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
    p.path = p.requested;
    if (p.history.size() >= 2 && p.history.at(p.history.size() - 2) == p.path)
        p.history.removeLast();
    else if (p.history.isEmpty() || p.history.last() != p.path)
        p.history.append(p.path);
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(entries.begin(), entries.end(), [&collator](const TransferEntry& a, const TransferEntry& b) {
        return a.directory != b.directory ? a.directory : collator.compare(a.name, b.name) < 0;
    });
    const auto folder = style()->standardIcon(p.path.isEmpty() ? QStyle::SP_DriveHDIcon : QStyle::SP_DirIcon);
    const auto file = style()->standardIcon(QStyle::SP_FileIcon);
    for (const auto& entry : entries) {
        auto* item = new QTreeWidgetItem(p.files);
        item->setText(1, entry.name);
        item->setToolTip(1, nativePath(entry.path));
        item->setIcon(1, entry.directory ? folder : file);
        const auto suffix = QFileInfo(entry.name).suffix();
        item->setText(3, p.path.isEmpty() ? tr("Drive") : entry.directory ? tr("Folder") : suffix.isEmpty() ? tr("File") : suffix);
        if (!p.path.isEmpty() && entry.modified.isValid())
            item->setText(2, entry.modified.toLocalTime().toString(QStringLiteral("yyyy/MM/dd HH:mm")));
        if (!entry.directory)
            item->setText(4, locale().formattedDataSize(entry.size));
        item->setData(0, Qt::UserRole, QVariant::fromValue(entry));
        item->setSelected(selected.contains(entry.name));
    }
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
        const bool hasSelection = destination && !p.files->selectedItems().isEmpty();
        p.back->setEnabled(available && (p.history.size() > 1 || (!p.ready && !p.history.isEmpty())));
        p.up->setEnabled(destination);
        p.refresh->setEnabled(available);
        p.locations->setEnabled(available);
        p.newFolder->setEnabled(destination);
        const int count = p.files->topLevelItemCount();
        const int selected = p.files->selectedItems().size();
        p.selectAll->setEnabled(p.ready && count > 0);
        p.selectAll->setCheckState(!selected ? Qt::Unchecked : selected == count ? Qt::Checked : Qt::PartiallyChecked);
        p.copy->setEnabled(hasSelection);
        p.paste->setEnabled(destination && (ours ? m_CopiedRemote != bool(side) : side && mime && mime->hasUrls()));
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
    auto bookmarks = QSettings().value(p.settingsKey).toStringList();
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
    auto* bookmark = menu.addAction(bookmarks.contains(p.path) ? tr("Remove current bookmark") : tr("Bookmark current folder"));
    bookmark->setEnabled(p.ready && !p.path.isEmpty());
    auto* anchor = p.address->parentWidget();
    auto* chosen = menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
    if (chosen == bookmark) {
        if (bookmarks.contains(p.path)) bookmarks.removeAll(p.path);
        else bookmarks.append(p.path);
        QSettings().setValue(p.settingsKey, bookmarks);
    }
    else if (chosen) navigate(remote, chosen->data().toString());
}

void FileTransferWindow::copySelection(bool remote)
{
    m_Copied = selection(remote);
    if (m_Copied.isEmpty())
        return;
    m_CopiedRemote = remote;
    auto* mime = new QMimeData;
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
    }
    else if (remote && mime->hasUrls()) {
        QList<TransferEntry> entries;
        for (const auto& url : mime->urls()) {
            if (!url.isLocalFile())
                continue;
            QFileInfo info(url.toLocalFile());
            if (!info.exists() || info.isRoot() || info.isSymLink())
                continue;
            TransferEntry entry;
            entry.name = info.fileName();
            entry.path = info.absoluteFilePath();
            entry.directory = info.isDir();
            entry.size = info.size();
            entry.modified = info.lastModified();
            entries.append(entry);
        }
        transfer(false, entries);
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
    auto* item = new QTreeWidgetItem(m_Queue);
    m_Tasks.insert(item, Task{entry, destination, key, fromRemote, bytes});
    item->setText(0, entry.name);
    item->setText(1, tr("Interrupted"));
    item->setText(2, byteCounts(locale(), bytes, entry.directory ? -1 : entry.size));
    const auto source = fromRemote ? tr("Remote · %1") : tr("Local · %1");
    const auto target = fromRemote ? tr("Local · %1") : tr("Remote · %1");
    item->setText(4, source.arg(nativePath(entry.path)));
    item->setText(5, target.arg(nativePath(destination)));
    item->setToolTip(4, item->text(4));
    item->setToolTip(5, item->text(5));
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

void FileTransferWindow::startTask(QTreeWidgetItem* item)
{
    auto& t = m_Tasks[item];
    if (t.state != State::Interrupted || !m_Available || !takeTask(item)) return;
    t.state = State::Active;
    t.retryable = false;
    saveTask(item);
    t.id = m_Engine->enqueue(t.fromRemote, t.entry, t.destination);
    m_Running.insert(t.id, item);
    item->setText(1, tr("Queued"));
    item->setToolTip(1, {});
    item->setText(3, {});
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
    for (const auto& entry : entries) {
        const auto key = taskKey(fromRemote, entry.path, destination.path);
        auto it = std::find_if(m_Tasks.cbegin(), m_Tasks.cend(), [&](const Task& t) {
            return t.key == key && t.state != State::Finished;
        });
        QTreeWidgetItem* item;
        if (it != m_Tasks.cend()) {
            item = it.key();
            if (it->state == State::Interrupted) m_Tasks[item].entry = entry;
            m_Queue->setCurrentItem(item);
            m_Queue->scrollToItem(item);
        } else {
            item = addTask(fromRemote, entry, destination.path, key, 0);
        }
        startTask(item);
    }
    updateActions();
}

void FileTransferWindow::closeEvent(QCloseEvent* event)
{
    if (m_Standalone && !m_Running.isEmpty()) {
        // Closing the browser lets queued transfers finish in the background.
        m_CloseAfterTransfers = true;
        hide();
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
}

void FileTransferWindow::refreshSpeed()
{
    auto* item = m_Running.value(m_SpeedJob);
    if (!item) return;
    const auto rate = m_Speed.sample(m_AttemptBytes);
    item->setText(3, rate < 0 ? QString() : tr("%1/s").arg(locale().formattedDataSize(rate)));
}

void FileTransferWindow::showProgress(int id, qint64 bytes, qint64 total, const QString& status, bool finished)
{
    auto* item = m_Running.value(id);
    if (!item) return;
    auto& t = m_Tasks[item];
    t.bytes = bytes;
    item->setText(1, finished && t.retryable && t.state != State::Cancelling ? tr("Interrupted") : status);
    item->setToolTip(1, status);
    item->setText(2, byteCounts(locale(), bytes, total));
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
        item->setText(3, {});
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
    if (m_Panes[remoteDestination].requested == t.destination) m_Engine->list(remoteDestination, t.destination);
    updateActions();
    if (m_CloseAfterTransfers && !isVisible() && m_Running.isEmpty())
        QTimer::singleShot(0, this, [this] { if (!isVisible()) close(); });
}
