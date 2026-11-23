#include "fileconflict.h"
#include "filetransferstyle.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {
class ElidedLabel final : public QLabel
{
public:
    ElidedLabel(const QString& text, QWidget* parent) : QLabel(text, parent), m_FullText(text)
    {
        setTextFormat(Qt::PlainText);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setToolTip(text);
        setAccessibleName(text);
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QLabel::resizeEvent(event);
        setText(fontMetrics().elidedText(m_FullText, Qt::ElideMiddle, contentsRect().width()));
    }

private:
    const QString m_FullText;
};

class FileConflictDialog final : public QDialog
{
    Q_DECLARE_TR_FUNCTIONS(FileConflictDialog)
public:
    FileConflictDialog(QWidget* parent, bool remote, int itemCount, const QList<FileConflict>& conflicts)
        : QDialog(parent), m_Conflicts(conflicts), m_Choices(conflicts.size(), FileConflictChoice::Skip)
    {
        setWindowTitle(tr("Replace or Skip Files"));
        setWindowFlag(Qt::WindowContextHelpButtonHint, false);
        setAttribute(Qt::WA_DeleteOnClose);
        if (!parent) setWindowFlag(Qt::WindowStaysOnTopHint);
        FileTransferStyle::apply(this);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 16, 20, 16);
        layout->setSpacing(10);

        auto* header = new QVBoxLayout;
        header->setSpacing(4);
        const auto heading = conflicts.size() == 1 ? tr("The destination already contains “%1”.").arg(QFileInfo(conflicts.first().path).fileName())
                                                    : tr("The destination has %1 files with the same names.").arg(conflicts.size());
        auto* title = new ElidedLabel(heading, this);
        title->setFont(FileTransferStyle::headingFont(title->font()));
        header->addWidget(title);
        auto folder = QFileInfo(conflicts.first().path).absolutePath();
        for (const auto& conflict : conflicts) {
            const auto path = QDir::fromNativeSeparators(conflict.path);
            while (!path.startsWith(folder.endsWith('/') ? folder : folder + '/', Qt::CaseInsensitive)) {
                const auto parentPath = QFileInfo(folder).absolutePath();
                if (parentPath == folder) break;
                folder = parentPath;
            }
        }
        const auto location = (remote ? tr("Remote · %1") : tr("Local · %1")).arg(QDir::toNativeSeparators(folder));
        const auto context = itemCount == 1 ? tr("Copying 1 item to %1").arg(location)
                                            : tr("Copying %1 items to %2").arg(itemCount).arg(location);
        auto* destination = new ElidedLabel(context, this);
        destination->setObjectName("transferContext");
        header->addWidget(destination);
        layout->addLayout(header);
        const auto blocked = std::find_if(conflicts.cbegin(), conflicts.cend(), [](const FileConflict& f) {
            return !f.replaceUnavailable.isEmpty();
        });
        if (blocked != conflicts.cend()) {
            label(conflicts.size() == 1 ? blocked->replaceUnavailable : tr("Some files cannot be replaced. See details for each file."), layout);
        }

        m_Actions = new QWidget(this);
        auto* actions = new QVBoxLayout(m_Actions);
        actions->setContentsMargins(0, 2, 0, 2);
        actions->setSpacing(6);
        auto* replace = action(actions, conflicts.size() == 1 ? tr("&Replace the file in the destination") : tr("&Replace the files in the destination"),
                               QStringLiteral("document-revert"), QStyle::SP_DialogApplyButton);
        replace->setEnabled(blocked == conflicts.cend());
        auto* skip = action(actions, conflicts.size() == 1 ? tr("&Skip this file") : tr("&Skip these files"),
                            QStringLiteral("go-next"), QStyle::SP_ArrowForward);
        auto* keep = action(actions, tr("&Keep both copies"), QStringLiteral("edit-copy"), QStyle::SP_FileIcon);
        connect(replace, &QPushButton::clicked, this, [this] { finishAll(FileConflictChoice::Replace); });
        connect(skip, &QPushButton::clicked, this, [this] { finishAll(FileConflictChoice::Skip); });
        connect(keep, &QPushButton::clicked, this, [this] { finishAll(FileConflictChoice::KeepBoth); });
        m_Individual = action(actions, tr("&Let me decide for each file"), QStringLiteral("edit-select-all"), QStyle::SP_FileDialogListView);
        connect(m_Individual, &QPushButton::clicked, this, [this] { setIndividual(true); });
        layout->addWidget(m_Actions);
        auto* line = new QFrame(this);
        line->setObjectName("transferDivider");
        line->setFixedHeight(1);
        layout->addWidget(line);

        m_Details = new QToolButton(this);
        m_Details->setObjectName("transferTool");
        m_Details->setText(tr("Details"));
        m_Details->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_Details->setArrowType(Qt::RightArrow);
        m_Details->setCheckable(true);
        m_Details->setAutoRaise(true);
        layout->addWidget(m_Details, 0, Qt::AlignLeft);
        m_Files = new QTreeWidget(this);
        FileTransferStyle::configureList(m_Files, {tr("File name"), tr("Action")});
        m_Files->setTextElideMode(Qt::ElideMiddle);
        m_Files->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_Files->setColumnHidden(1, true);
        const auto fileIcon = style()->standardIcon(QStyle::SP_FileIcon);
        for (const auto& conflict : conflicts) {
            auto* row = new QTreeWidgetItem(m_Files, {QDir::toNativeSeparators(QDir(folder).relativeFilePath(conflict.path))});
            row->setIcon(0, fileIcon);
            row->setToolTip(0, QDir::toNativeSeparators(conflict.path));
        }
        layout->addWidget(m_Files, 1);
        m_Reason = label({}, layout);
        connect(m_Files, &QTreeWidget::currentItemChanged, this, [this] { updateReason(); });
        m_Files->setCurrentItem(m_Files->topLevelItem(0));
        m_Files->hide();
        m_Reason->hide();
        connect(m_Details, &QToolButton::toggled, this, [this](bool checked) {
            m_Details->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
            m_Files->setVisible(checked);
            updateReason();
            fitContents(width());
        });

        m_Footer = new QWidget(this);
        auto* footer = new QHBoxLayout(m_Footer);
        footer->setContentsMargins(0, 0, 0, 0);
        footer->addStretch();
        auto* back = new QPushButton(tr("Back"), m_Footer);
        auto* apply = new QPushButton(tr("Continue"), m_Footer);
        apply->setObjectName("transferPrimary");
        back->setAutoDefault(false);
        apply->setAutoDefault(false);
        footer->addWidget(back);
        footer->addWidget(apply);
        connect(back, &QPushButton::clicked, this, [this] { setIndividual(false); });
        connect(apply, &QPushButton::clicked, this, [this] {
            for (int i = 0; i < m_Selectors.size(); ++i)
                m_Choices[i] = static_cast<FileConflictChoice>(m_Selectors[i]->currentData().toInt());
            accept();
        });
        layout->addWidget(m_Footer);
        m_Footer->hide();
        skip->setFocus();
        const int preferredWidth = qMax(420, fontMetrics().horizontalAdvance(QLatin1Char('M')) * 40);
        fitContents(qMin(preferredWidth, screen()->availableGeometry().width() - 32));
    }

    const QList<FileConflictChoice>& choices() const { return m_Choices; }

private:
    QLabel* label(const QString& text, QVBoxLayout* layout)
    {
        auto* result = new QLabel(this);
        result->setTextFormat(Qt::PlainText);
        result->setWordWrap(true);
        result->setText(text);
        layout->addWidget(result);
        return result;
    }

    QPushButton* action(QVBoxLayout* layout, const QString& text, const QString& iconName, QStyle::StandardPixmap fallback)
    {
        auto* button = new QPushButton(FileTransferStyle::icon(this, iconName, fallback), text, this);
        button->setObjectName("transferCommand");
        const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, button);
        button->setIconSize(QSize(iconSize, iconSize));
        button->setAutoDefault(true);
        layout->addWidget(button);
        return button;
    }

    void finishAll(FileConflictChoice choice)
    {
        m_Choices.fill(choice);
        accept();
    }

    void fitContents(int targetWidth)
    {
        if (!m_Files->isHidden()) {
            const int visibleRows = qMin(m_Files->topLevelItemCount(), 5);
            const int rowHeight = m_Files->sizeHintForRow(0);
            m_Files->setMinimumHeight(m_Files->header()->sizeHint().height() + visibleRows * rowHeight + 2 * m_Files->frameWidth());
        }
        layout()->activate();
        const int height = layout()->hasHeightForWidth() ? layout()->totalMinimumHeightForWidth(targetWidth) : minimumSizeHint().height();
        resize(targetWidth, height);
    }

    void updateReason()
    {
        const auto i = m_Files->indexOfTopLevelItem(m_Files->currentItem());
        const auto reason = i >= 0 ? m_Conflicts.at(i).replaceUnavailable : QString();
        m_Reason->setText(reason);
        m_Reason->setVisible(!m_Files->isHidden() && !reason.isEmpty());
    }

    void setIndividual(bool individual)
    {
        if (individual && m_Selectors.isEmpty()) {
            for (int i = 0; i < m_Conflicts.size(); ++i) {
                auto* box = new QComboBox(m_Files);
                auto* row = m_Files->topLevelItem(i);
                box->setAccessibleName(tr("Action for %1").arg(row->text(0)));
                box->addItem(tr("Skip"), int(FileConflictChoice::Skip));
                if (m_Conflicts[i].replaceUnavailable.isEmpty())
                    box->addItem(tr("Replace"), int(FileConflictChoice::Replace));
                box->addItem(tr("Keep both copies"), int(FileConflictChoice::KeepBoth));
                m_Files->setItemWidget(row, 1, box);
                m_Selectors.append(box);
            }
        }
        m_Actions->setVisible(!individual);
        m_Details->setVisible(!individual);
        m_Files->setColumnHidden(1, !individual);
        m_Files->setVisible(individual || m_Details->isChecked());
        m_Footer->setVisible(individual);
        updateReason();
        if (individual) m_Selectors.first()->setFocus();
        else m_Individual->setFocus();
        fitContents(width());
    }

    QList<FileConflict> m_Conflicts;
    QList<FileConflictChoice> m_Choices;
    QWidget* m_Actions;
    QPushButton* m_Individual = nullptr;
    QToolButton* m_Details;
    QTreeWidget* m_Files;
    QLabel* m_Reason;
    QWidget* m_Footer;
    QList<QComboBox*> m_Selectors;
};
}

QDialog* showFileConflicts(QWidget* parent, bool remote, int itemCount, const QList<FileConflict>& conflicts,
                           std::function<void(const QList<FileConflictChoice>&)> reply)
{
    Q_ASSERT(!conflicts.isEmpty() && itemCount > 0);
    if (parent && (!parent->isVisible() || parent->isMinimized())) parent->showNormal();
    auto* dialog = new FileConflictDialog(parent, remote, itemCount, conflicts);
    QObject::connect(dialog, &QDialog::finished, dialog, [dialog, reply = std::move(reply)] { reply(dialog->choices()); });
    dialog->open();
    if (!parent) {
        dialog->raise();
        dialog->activateWindow();
    }
    return dialog;
}
