#include "streaming/filetransferwindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QSslCertificate>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
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
        QTest::newRow("skip") << QString("Skip") << false;
        QTest::newRow("escape skips") << QString("Escape") << false;
        QTest::newRow("replace") << QString("Replace") << false;
        QTest::newRow("keep both with occupied suffix") << QString("Keep both") << false;
        QTest::newRow("copy onto itself") << QString("Keep both") << true;
        QTest::newRow("no conflict") << QString() << false;
    }

    void paste()
    {
        QFETCH(QString, choice);
        QFETCH(bool, sameFile);
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
        mime->setUrls({QUrl::fromLocalFile(input), QUrl::fromLocalFile(next)});
        QApplication::clipboard()->setMimeData(mime);
        int prompts = 0;
        bool expectedButtons = false;
        QString error;
        QTimer answer;
        connect(&answer, &QTimer::timeout, this, [&] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (!box) return;
            const auto buttons = box->buttons();
            QHash<QString, QAbstractButton*> byText;
            for (auto* button : buttons) byText.insert(button->text(), button);
            if (!byText.contains("Skip") || !byText.contains("Replace") || !byText.contains("Keep both")) {
                error = box->text();
                box->reject();
                return;
            }
            ++prompts;
            expectedButtons = box->text().contains("models.json") && byText["Replace"]->isEnabled() != sameFile &&
                              box->defaultButton() == byText["Skip"] && box->escapeButton() == byText["Skip"];
            if (choice == "Escape" || !byText.contains(choice)) QTest::keyClick(box, Qt::Key_Escape);
            else byText[choice]->click();
        });
        answer.start(10);
        local->setFocus();
        QTest::keyClick(local, Qt::Key_V, Qt::ControlModifier);
        QTRY_VERIFY(!error.isEmpty() || !local->findItems("next.txt", Qt::MatchExactly, 1).isEmpty());
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(prompts, choice.isEmpty() ? 0 : 1);
        QVERIFY(!prompts || expectedButtons);
        QCOMPARE(readFile(input), "new");
        QCOMPARE(readFile(target), sameFile || choice.isEmpty() || choice == "Replace" ? "new" : "old");
        QCOMPARE(readFile(destination.filePath("models - Copy.json")), "occupied");
        const auto copy = destination.filePath("models - Copy (2).json");
        QCOMPARE(QFileInfo::exists(copy), choice == "Keep both");
        if (choice == "Keep both") QCOMPARE(readFile(copy), "new");
        QCOMPARE(readFile(destination.filePath("next.txt")), "next");
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
            [&](bool remote, const QString& path, const QString&, FileTransferEngine::ConflictReply reply) {
                edited = !remote && path == target && writeFile(target, "edited while choosing");
                reply(FileTransferEngine::ConflictChoice::Replace);
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
};

QTEST_MAIN(FileConflictTest)
#include "main.moc"
