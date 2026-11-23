#include "streaming/filetransferwindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSslCertificate>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUuid>
#include <QtTest>

namespace {
bool writeFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size() && file.flush();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QPushButton* button(QDialog* dialog, const QString& text)
{
    for (auto* button : dialog->findChildren<QPushButton*>())
        if (button->text().remove('&') == text) return button;
    return nullptr;
}

bool hasLabel(QDialog* dialog, const QString& text)
{
    for (auto* label : dialog->findChildren<QLabel*>())
        if (label->isVisible() && label->text().contains(text)) return true;
    return false;
}
}

class FileConflictTest : public QObject
{
    Q_OBJECT
    QTemporaryDir settings;

private slots:
    void initTestCase()
    {
        QVERIFY(settings.isValid());
        QCoreApplication::setOrganizationName("Moonlight tests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    }

    void paste_data()
    {
        QTest::addColumn<QString>("choice");
        QTest::addColumn<bool>("sameFile");
        QTest::addColumn<bool>("batch");
        QTest::newRow("skip") << QString("Skip") << false << false;
        QTest::newRow("escape skips") << QString("Escape") << false << false;
        QTest::newRow("replace") << QString("Replace") << false << false;
        QTest::newRow("keep both with occupied suffix") << QString("Keep both") << false << false;
        QTest::newRow("copy onto itself") << QString("Keep both") << true << false;
        QTest::newRow("no conflict") << QString() << false << false;
        QTest::newRow("batch skip") << QString("Skip") << false << true;
        QTest::newRow("batch replace") << QString("Replace") << false << true;
        QTest::newRow("batch individual") << QString("Individual") << false << true;
    }

    void paste()
    {
        QFETCH(QString, choice);
        QFETCH(bool, sameFile);
        QFETCH(bool, batch);
        QTemporaryDir source, destination;
        QVERIFY(source.isValid() && destination.isValid());
        const auto target = destination.filePath("models.json");
        const auto input = sameFile ? target : source.filePath("models.json");
        const auto next = source.filePath("next.txt");
        if (!choice.isEmpty()) QVERIFY(writeFile(target, "old"));
        QVERIFY(writeFile(input, "new"));
        QVERIFY(writeFile(next, "next"));
        QVERIFY(writeFile(destination.filePath("models - Copy.json"), "occupied"));

        FileTransferWindow window(QUrl("https://localhost"), QSslCertificate(), {}, "Host", "conflict-fixture");
        window.show();
        QTreeWidget* local = nullptr;
        QLineEdit* address = nullptr;
        for (auto* tree : window.findChildren<QTreeWidget*>()) {
            auto* field = tree->parentWidget()->findChild<QLineEdit*>();
            if (field && field->isEnabled()) {
                local = tree;
                address = field;
                break;
            }
        }
        QVERIFY(local && address);
        address->setText(destination.path());
        QTest::keyClick(address, Qt::Key_Return);
        QTRY_VERIFY(!local->findItems("models - Copy.json", Qt::MatchExactly, 1).isEmpty());

        auto* mime = new QMimeData;
        QList<QUrl> urls{QUrl::fromLocalFile(input), QUrl::fromLocalFile(next)};
        const auto other = destination.filePath("other.txt");
        if (batch) {
            QVERIFY(writeFile(other, "other old"));
            QVERIFY(writeFile(source.filePath("other.txt"), "other new"));
            urls.append(QUrl::fromLocalFile(source.filePath("other.txt")));
        }
        mime->setUrls(urls);
        QApplication::clipboard()->setMimeData(mime);
        int prompts = 0;
        bool expectedButtons = false;
        QString error;
        QTimer answer;
        connect(&answer, &QTimer::timeout, this, [&] {
            auto* box = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!box) return;
            auto* skip = button(box, batch ? "Skip these files" : "Skip this file");
            auto* replace = button(box, batch ? "Replace the files in the destination" : "Replace the file in the destination");
            auto* keep = button(box, "Keep both copies");
            auto* each = button(box, "Let me decide for each file");
            if (!skip || !replace || !keep || !each) {
                error = box->windowTitle();
                box->reject();
                return;
            }
            ++prompts;
            expectedButtons = hasLabel(box, batch ? "2 files" : "models.json") &&
                              replace->isEnabled() != sameFile && skip->isDefault();
            if (choice == "Replace") replace->click();
            else if (choice == "Keep both") keep->click();
            else if (choice == "Skip") skip->click();
            else if (choice == "Individual") {
                each->click();
                auto* list = box->findChild<QTreeWidget*>();
                auto* apply = button(box, "Continue");
                if (!list || !apply || list->topLevelItemCount() != 2) {
                    error = box->windowTitle();
                    box->reject();
                    return;
                }
                for (int i = 0; i < list->topLevelItemCount(); ++i) {
                    auto* row = list->topLevelItem(i);
                    auto* select = qobject_cast<QComboBox*>(list->itemWidget(row, 1));
                    if (row->text(0) == "models.json") select->setCurrentIndex(select->findData(int(FileConflictChoice::Replace)));
                }
                apply->click();
            } else QTest::keyClick(box, Qt::Key_Escape);
        });
        answer.start(10);
        local->setFocus();
        QTest::keyClick(local, Qt::Key_V, Qt::ControlModifier);
        QTRY_VERIFY(!error.isEmpty() || !local->findItems("next.txt", Qt::MatchExactly, 1).isEmpty());
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(prompts, choice.isEmpty() ? 0 : 1);
        QVERIFY(!prompts || expectedButtons);
        QCOMPARE(readFile(input), "new");
        QCOMPARE(readFile(target), sameFile || choice.isEmpty() || choice == "Replace" || choice == "Individual" ? "new" : "old");
        if (batch) QTRY_COMPARE(readFile(other), choice == "Replace" ? "other new" : "other old");
        QCOMPARE(readFile(destination.filePath("models - Copy.json")), "occupied");
        const auto copy = destination.filePath("models - Copy (2).json");
        QCOMPARE(QFileInfo::exists(copy), choice == "Keep both");
        if (choice == "Keep both") QCOMPARE(readFile(copy), "new");
        QCOMPARE(readFile(destination.filePath("next.txt")), "next");
    }

    void batchDialog_data()
    {
        QTest::addColumn<QString>("action");
        for (const char* action : {"Replace", "Skip", "Keep both", "Individual", "Escape"})
            QTest::newRow(action) << QString(action);
    }

    void batchDialog()
    {
        QFETCH(QString, action);
        using Choice = FileConflictChoice;
        QTemporaryDir folder;
        const bool mixed = action == "Individual";
        QList<FileConflict> conflicts{{folder.filePath("一.txt"), {}}, {folder.filePath("two"), mixed ? "A folder occupies this name." : ""},
                                      {folder.filePath("three.csv"), {}}};
        QList<Choice> answer;
        auto* dialog = showFileConflicts(nullptr, false, 5, conflicts, [&](const auto& choices) { answer = choices; });
        QVERIFY(hasLabel(dialog, "5 items") && hasLabel(dialog, "3 files"));
        auto* list = dialog->findChild<QTreeWidget*>();
        auto* details = dialog->findChild<QToolButton*>();
        QVERIFY(list && details && !list->isVisible());
        details->click();
        QVERIFY(list->isVisible() && list->isColumnHidden(1));
        QCOMPARE(list->topLevelItemCount(), 3);
        QCOMPARE(list->topLevelItem(0)->text(0), QStringLiteral("一.txt"));
        auto* replace = button(dialog, "Replace the files in the destination");
        auto* skip = button(dialog, "Skip these files");
        auto* keep = button(dialog, "Keep both copies");
        auto* each = button(dialog, "Let me decide for each file");
        QVERIFY(replace && skip && keep && each);
        QCOMPARE(replace->isEnabled(), !mixed);
        details->click();
        if (mixed) {
            const int width = dialog->width() + 64;
            dialog->resize(width, dialog->height());
            each->click();
            QVERIFY(list->isVisible() && !list->isColumnHidden(1));
            QCOMPARE(dialog->width(), width);
            const auto* apply = button(dialog, "Continue");
            QVERIFY(apply);
            QTRY_VERIFY(list->geometry().bottom() < apply->mapTo(dialog, QPoint()).y());
            list->setCurrentItem(list->topLevelItem(1));
            QVERIFY(hasLabel(dialog, "A folder occupies this name."));
            const QList<Choice> wanted{Choice::Replace, Choice::Skip, Choice::KeepBoth};
            for (int i = 0; i < wanted.size(); ++i) {
                auto* select = qobject_cast<QComboBox*>(list->itemWidget(list->topLevelItem(i), 1));
                QVERIFY(select);
                QCOMPARE(select->findData(int(Choice::Replace)) >= 0, i != 1);
                select->setCurrentIndex(select->findData(int(wanted[i])));
            }
            button(dialog, "Back")->click();
            QVERIFY(!list->isVisible() && each->isVisible());
            QCOMPARE(dialog->width(), width);
            each->click();
            button(dialog, "Continue")->click();
            QCOMPARE(answer, wanted);
        } else {
            Choice expected = Choice::Skip;
            if (action == "Replace") { expected = Choice::Replace; replace->click(); }
            else if (action == "Keep both") { expected = Choice::KeepBoth; keep->click(); }
            else if (action == "Escape") QTest::keyClick(dialog, Qt::Key_Escape);
            else skip->click();
            QCOMPARE(answer, QList<Choice>(3, expected));
        }
    }

    void destinationChangedWhileChoosing()
    {
        QTemporaryDir source, destination;
        QVERIFY(source.isValid() && destination.isValid());
        const auto input = source.filePath("models.json");
        const auto target = destination.filePath("models.json");
        QVERIFY(writeFile(input, "new") && writeFile(target, "old"));
        bool edited = false;
        FileTransferEngine engine({}, QSslCertificate(), {}, nullptr, false,
            [&](bool remote, int, const QList<FileConflict>& conflicts, FileTransferEngine::ConflictReply reply) {
                edited = !remote && conflicts.size() == 1 && conflicts.first().path == target &&
                         writeFile(target, "edited while choosing");
                reply({FileConflictChoice::Replace});
            });
        QString error;
        bool finished = false;
        engine.manageFiles(FileTransferEngine::FileAction::Copy, false,
                           {{"models.json", input, false, 3, QFileInfo(input).lastModified()}}, destination.path(),
                           [&](const QString& result) { error = result; finished = true; });
        QTRY_VERIFY(finished);
        QVERIFY(edited && !error.isEmpty());
        QCOMPARE(readFile(target), "edited while choosing");
        QCOMPARE(QDir(destination.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot),
                 QStringList{"models.json"});
    }

    void mergeAndDeleteFolder()
    {
        QTemporaryDir source, destination;
        QVERIFY(source.isValid() && destination.isValid());
        const QString name = "photos-" + QUuid::createUuid().toString(QUuid::Id128);
        const auto input = source.filePath(name);
        const auto output = destination.filePath(name);
        QVERIFY(QDir().mkpath(input + "/day/empty") && QDir().mkpath(output + "/day"));
        QVERIFY(writeFile(input + "/day/new.txt", "new") && writeFile(output + "/untouched", "old"));
        QVERIFY(writeFile(input + "/day/same", "source") && writeFile(output + "/day/same", "destination"));
        QVERIFY(writeFile(input + "/keep.txt", "source") && writeFile(output + "/keep.txt", "destination"));
        int prompts = 0;
        FileTransferEngine engine({}, QSslCertificate(), {}, nullptr, false,
            [&](bool remote, int itemCount, const QList<FileConflict>& conflicts, FileTransferEngine::ConflictReply reply) {
                ++prompts;
                reply(QList<FileConflictChoice>(conflicts.size(), FileConflictChoice::Skip));
                QVERIFY(!remote);
                QCOMPARE(itemCount, 1);
                QStringList paths;
                for (const auto& conflict : conflicts) {
                    QVERIFY(conflict.replaceUnavailable.isEmpty());
                    paths.append(conflict.path);
                }
                paths.sort();
                QCOMPARE(paths, (QStringList{output + "/day/same", output + "/keep.txt"}));
            });
        const TransferEntry folder{name, input, true, 0, QFileInfo(input).lastModified()};
        QString error;
        bool done = false;
        auto finished = [&](const QString& message) { error = message; done = true; };
        engine.manageFiles(FileTransferEngine::FileAction::Copy, false, {folder}, destination.path(), finished);
        QTRY_VERIFY(done);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(prompts, 1);
        QVERIFY(QFileInfo(output + "/day/empty").isDir());
        QCOMPARE(readFile(output + "/day/new.txt"), "new");
        QCOMPARE(readFile(output + "/day/same"), "destination");
        QCOMPARE(readFile(output + "/untouched"), "old");
        done = false;
        engine.manageFiles(FileTransferEngine::FileAction::Copy, false, {folder}, input + "/day", finished);
        QTRY_VERIFY(done);
        QVERIFY(!error.isEmpty() && !QFileInfo::exists(input + "/day/" + name));
        FileTransferWindow window({}, QSslCertificate(), {}, "Host", "folder-delete-fixture");
        window.show();
        QTreeWidget* local = nullptr;
        for (auto* tree : window.findChildren<QTreeWidget*>()) {
            auto* address = tree->parentWidget()->findChild<QLineEdit*>();
            if (address && address->isEnabled()) {
                local = tree;
                address->setText(destination.path());
                QTest::keyClick(address, Qt::Key_Return);
                break;
            }
        }
        QVERIFY(local);
        QTRY_VERIFY(!local->findItems(name, Qt::MatchExactly, 1).isEmpty());
        local->setCurrentItem(local->findItems(name, Qt::MatchExactly, 1).first());
        local->setFocus();
        QAction* remove = nullptr;
        for (auto* action : local->actions())
            if (action->shortcut().matches(QKeySequence::Delete) == QKeySequence::ExactMatch) remove = action;
        QVERIFY(remove && remove->isEnabled());
        for (auto answer : {QMessageBox::Cancel, QMessageBox::Yes}) {
            bool prompted = false;
            QTimer respond;
            connect(&respond, &QTimer::timeout, this, [&] {
                if (auto* box = window.findChild<QMessageBox*>()) {
                    prompted = box->text().contains("Recycle Bin") && box->informativeText().contains(name);
                    if (box->button(answer)) box->button(answer)->click();
                    else box->reject();
                }
            });
            respond.start(5);
            if (answer == QMessageBox::Cancel) {
                window.activateWindow();
                local->setFocus();
                QTRY_VERIFY(local->hasFocus());
                QTest::keyClick(local, Qt::Key_Delete);
            } else remove->trigger();
            QVERIFY(prompted);
            if (answer == QMessageBox::Cancel) QVERIFY(QFileInfo::exists(output));
        }
        QTRY_VERIFY(!QFileInfo::exists(output));
        QCOMPARE(readFile(input + "/day/new.txt"), "new");
    }
};

QTEST_MAIN(FileConflictTest)
#include "main.moc"
