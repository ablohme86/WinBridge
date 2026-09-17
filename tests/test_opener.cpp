/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <QtTest>
#include <QtWidgets>
#include "opener.h"
#include "single_instance.h"

using namespace WinBridge;

class OpenerTests : public QObject {
    Q_OBJECT

private slots:
    void activationChannelReusesRunningWindow() {
        const QString id = "test-" + QUuid::createUuid().toString(QUuid::Id128).left(8);
        bool activated = false;
        InstanceActivationServer server(id, [&](const QString &) { activated = true; });
        if (!server.start()) QSKIP("Local sockets are unavailable in this sandbox.");
        QVERIFY(activateRunningInstance(id));
        QTRY_VERIFY_WITH_TIMEOUT(activated, 2000);
    }
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

        const QString shortcut = tmp.filePath("Shortcut.lnk");
        QFile scFile(shortcut);
        QVERIFY(scFile.open(QIODevice::WriteOnly));
        scFile.write("shortcut");
        scFile.close();
        QVERIFY(!rememberExecutable(shortcut, storage));
        QCOMPARE(recentExecutables(storage), QStringList({second}));

        QFile jsonFile(storage);
        QVERIFY(jsonFile.open(QIODevice::WriteOnly));
        jsonFile.write(QJsonDocument(QJsonArray{shortcut, second}).toJson());
        jsonFile.close();
        QCOMPARE(recentExecutables(storage), QStringList({second}));
    }

    void managerIsFoundBesideLauncher() {
        QTemporaryDir tmp;
        const QString launcher = tmp.filePath("winbridge");
        const QString manager = tmp.filePath("winbridge-manager");
        for (const QString &path : {launcher, manager}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("#!/bin/sh\n");
            file.close();
            QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        }
        QCOMPARE(managerExecutablePath(launcher), manager);
    }

    void launcherWindowExposesFileRecentAndShortcutControls() {
        bool foundPath = false;
        bool foundRecents = false;
        bool foundShortcutMenu = false;
        bool foundOpen = false;
        bool foundManager = false;
        QTimer::singleShot(0, this, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            foundPath = dialog->findChild<QLineEdit *>("pathEdit") != nullptr;
            foundRecents = dialog->findChild<QListWidget *>("recentList") != nullptr;
            auto *shortcut = dialog->findChild<QPushButton *>("shortcutButton");
            foundShortcutMenu = shortcut && shortcut->menu() && shortcut->menu()->actions().size() == 2;
            foundOpen = dialog->findChild<QPushButton *>("primary") != nullptr;
            auto *manager = dialog->findChild<QPushButton *>("managerButton");
            foundManager = manager && !manager->icon().isNull();
            dialog->reject();
        });
        const OpenRequest request = showExecutableOpener("/usr/bin/winbridge");
        QVERIFY(!request.accepted);
        QVERIFY(foundPath);
        QVERIFY(foundRecents);
        QVERIFY(foundShortcutMenu);
        QVERIFY(foundOpen);
        QVERIFY(foundManager);
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
