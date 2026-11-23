#include "streaming/filetransferwindow.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QLineEdit>
#include <QSettings>
#include <QSslCertificate>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest>

class FileTransferSortTest : public QObject
{
    Q_OBJECT

private slots:
    void sortLocalFileAndFolder()
    {
        QTemporaryDir settings;
        QTemporaryDir directory;
        QVERIFY(settings.isValid() && directory.isValid());
        QCoreApplication::setOrganizationName("Moonlight tests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());

        const QString folderName = "Z-folder";
        const QString fileName = "a.bin";
        QVERIFY(QDir(directory.path()).mkdir(folderName));
        QFile file(directory.filePath(fileName));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("123"), 3);
        QVERIFY(file.flush());
        QVERIFY(file.setFileTime(QDateTime::fromSecsSinceEpoch(1600000000), QFileDevice::FileModificationTime));
        file.close();

        FileTransferWindow window(QUrl("https://localhost"), QSslCertificate(), {}, "Test host", "fixture");
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
        address->setText(directory.path());
        QTest::keyClick(address, Qt::Key_Return);
        QTRY_COMPARE(local->topLevelItemCount(), 2);
        QCOMPARE(local->topLevelItem(0)->text(1), fileName);

        auto* header = local->header();
        for (int column = 1; column <= 4; ++column) {
            // Name, modification time and type put a.bin first in ascending order.
            // A folder's blank size sorts before this three-byte file.
            const bool fileFirstAscending = column != 4;
            const QPoint center(header->sectionViewportPosition(column) + header->sectionSize(column) / 2,
                                header->viewport()->height() / 2);
            const auto click = header->viewport()->mapTo(&window, center);
            for (int i = 0; i < 2; ++i) {
                const auto before = header->sortIndicatorOrder();
                const bool sameColumn = header->sortIndicatorSection() == column;
                QTest::mouseClick(window.windowHandle(), Qt::LeftButton, {}, click);
                QCOMPARE(header->sortIndicatorSection(), column);
                if (sameColumn) QVERIFY(header->sortIndicatorOrder() != before);
                const bool fileFirst = (header->sortIndicatorOrder() == Qt::AscendingOrder) == fileFirstAscending;
                QTRY_COMPARE(local->topLevelItem(0)->text(1), fileFirst ? fileName : folderName);
                QCOMPARE(local->topLevelItem(1)->text(1), fileFirst ? folderName : fileName);
            }
        }
        QTest::keyClick(address, Qt::Key_Return);
        QTRY_COMPARE(local->topLevelItemCount(), 2);
        QCOMPARE(header->sortIndicatorSection(), 4);
        QCOMPARE(header->sortIndicatorOrder(), Qt::DescendingOrder);
        QCOMPARE(local->topLevelItem(0)->text(1), fileName);
    }
};

QTEST_MAIN(FileTransferSortTest)
#include "main.moc"
