/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <QtTest>
#include <QtWidgets>
#include "opener.h"

using namespace WinBridge;

class OpenerTests : public QObject {
    Q_OBJECT

private slots:
    void recentAppsAreUniqueNewestFirstAndDiscardMissingFiles() {
        QTemporaryDir tmp;
        const QString storage = tmp.filePath("config/recent.json");
        const QString first = tmp.filePath("First.exe");
        const QString second = tmp.filePath("Second.EXE");
        for (const QString &path : {first, second}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("MZ");
        }
        QVERIFY(rememberExecutable(first, storage));
        QVERIFY(rememberExecutable(second, storage));
        QVERIFY(rememberExecutable(first, storage));
        QCOMPARE(recentExecutables(storage), QStringList({first, second}));
        QVERIFY(QFile::remove(first));
        QCOMPARE(recentExecutables(storage), QStringList({second}));
    }

    void launcherWindowExposesFileRecentAndShortcutControls() {
        bool foundPath = false;
        bool foundRecents = false;
        bool foundShortcutMenu = false;
        bool foundOpen = false;
        QTimer::singleShot(0, this, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            foundPath = dialog->findChild<QLineEdit *>("pathEdit") != nullptr;
            foundRecents = dialog->findChild<QListWidget *>("recentList") != nullptr;
            auto *shortcut = dialog->findChild<QPushButton *>("shortcutButton");
            foundShortcutMenu = shortcut && shortcut->menu() && shortcut->menu()->actions().size() == 2;
            foundOpen = dialog->findChild<QPushButton *>("primary") != nullptr;
            dialog->reject();
        });
        const OpenRequest request = showExecutableOpener("/usr/bin/winbridge");
        QVERIFY(!request.accepted);
        QVERIFY(foundPath);
        QVERIFY(foundRecents);
        QVERIFY(foundShortcutMenu);
        QVERIFY(foundOpen);
    }

    void launcherWindowCanRenderScreenshot() {
        QTemporaryDir tmp;
        const QString screenshot = tmp.filePath("launcher.png");
        const OpenRequest request = showExecutableOpener(
            "/usr/bin/winbridge", nullptr, screenshot, "dark");
        QVERIFY(!request.accepted);
        QVERIFY(request.screenshotSaved);
        QImage image(screenshot);
        QVERIFY(!image.isNull());
        QVERIFY(image.width() >= 720);
        QVERIFY(image.height() >= 560);
    }
};

QTEST_MAIN(OpenerTests)
#include "test_opener.moc"
