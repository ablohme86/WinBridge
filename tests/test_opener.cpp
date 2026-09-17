/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <QtTest>
#include <QtWidgets>
#include <QDesktopServices>
#include "opener.h"
#include "single_instance.h"

#ifdef WINBRIDGE_HAVE_X11
#include <X11/Xlib.h>
#endif

using namespace WinBridge;

class FileUrlRecorder : public QObject {
    Q_OBJECT
public:
    QUrl opened;
public slots:
    void record(const QUrl &url) { opened = url; }
};

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

    void parseArgumentsSplitsCommandLineCorrectly() {
        QCOMPARE(parseArguments(""), QStringList());
        QCOMPARE(parseArguments("   "), QStringList());
        QCOMPARE(parseArguments("-flag /switch"), QStringList({"-flag", "/switch"}));
        QCOMPARE(parseArguments(R"(-path "C:\Program Files\Game" /fullscreen)"),
                 QStringList({"-path", "C:\\Program Files\\Game", "/fullscreen"}));
        QCOMPARE(parseArguments("-w 1920 -h 1080 -novid"),
                 QStringList({"-w", "1920", "-h", "1080", "-novid"}));
    }

    void launcherWindowExposesFileRecentAndShortcutControls() {
        bool foundPath = false;
        bool foundArgsCheck = false;
        bool foundArgs = false;
        bool argsInitiallyHidden = false;
        bool foundRecents = false;
        bool foundShortcutMenu = false;
        bool foundOpen = false;
        bool foundManager = false;
        QTimer::singleShot(0, this, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            foundPath = dialog->findChild<QLineEdit *>("pathEdit") != nullptr;
            auto *argsCheck = dialog->findChild<QCheckBox *>("argsCheck");
            foundArgsCheck = argsCheck != nullptr;
            auto *argsEdit = dialog->findChild<QLineEdit *>("argsEdit");
            foundArgs = argsEdit != nullptr;
            argsInitiallyHidden = argsEdit && !argsEdit->isVisible();
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
        QVERIFY(foundArgsCheck);
        QVERIFY(foundArgs);
        QVERIFY(argsInitiallyHidden);
        QVERIFY(foundRecents);
        QVERIFY(foundShortcutMenu);
        QVERIFY(foundOpen);
        QVERIFY(foundManager);
    }

    void launcherWindowReturnsExecutableAndOptionalArgumentsWhenAccepted() {
        QTemporaryDir tmp;
        const QString exe = tmp.filePath("game.exe");
        QFile file(exe);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("MZ");
        file.close();

        QTimer::singleShot(0, this, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto *pathEdit = dialog->findChild<QLineEdit *>("pathEdit");
            auto *argsCheck = dialog->findChild<QCheckBox *>("argsCheck");
            auto *argsEdit = dialog->findChild<QLineEdit *>("argsEdit");
            auto *openButton = dialog->findChild<QPushButton *>("primary");
            QVERIFY(pathEdit);
            QVERIFY(argsCheck);
            QVERIFY(argsEdit);
            QVERIFY(openButton);

            QVERIFY(!argsEdit->isVisible());
            argsCheck->setChecked(true);
            QVERIFY(argsEdit->isVisible());

            pathEdit->setText(exe);
            argsEdit->setText(R"(-fullscreen /debug "value with spaces")");
            QVERIFY(openButton->isEnabled());
            openButton->click();
        });

        const OpenRequest request = showExecutableOpener("/usr/bin/winbridge");
        QVERIFY(request.accepted);
        QCOMPARE(request.executable, exe);
        QCOMPARE(request.arguments, QStringList({"-fullscreen", "/debug", "value with spaces"}));
        QCOMPARE(request.argumentsText, QString(R"(-fullscreen /debug "value with spaces")"));
    }

    void launcherWindowIgnoresArgumentsWhenCheckboxIsUnchecked() {
        QTemporaryDir tmp;
        const QString exe = tmp.filePath("game.exe");
        QFile file(exe);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("MZ");
        file.close();

        QTimer::singleShot(0, this, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto *pathEdit = dialog->findChild<QLineEdit *>("pathEdit");
            auto *argsCheck = dialog->findChild<QCheckBox *>("argsCheck");
            auto *argsEdit = dialog->findChild<QLineEdit *>("argsEdit");
            auto *openButton = dialog->findChild<QPushButton *>("primary");
            QVERIFY(pathEdit);
            QVERIFY(argsCheck);
            QVERIFY(argsEdit);
            QVERIFY(openButton);

            argsCheck->setChecked(true);
            argsEdit->setText("-fullscreen");
            // Toggle off again
            argsCheck->setChecked(false);
            QVERIFY(!argsEdit->isVisible());

            pathEdit->setText(exe);
            openButton->click();
        });

        const OpenRequest request = showExecutableOpener("/usr/bin/winbridge");
        QVERIFY(request.accepted);
        QCOMPARE(request.executable, exe);
        QVERIFY(request.arguments.isEmpty());
        QVERIFY(request.argumentsText.isEmpty());
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

    void removeRecentExecutableRemovesEntryAndCleansUpFile() {
        QTemporaryDir tmp;
        const QString storage = tmp.filePath("config/recent.json");
        const QString first = tmp.filePath("First.exe");
        const QString second = tmp.filePath("Second.exe");
        for (const QString &path : {first, second}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("MZ");
            file.close();
        }
        QVERIFY(rememberExecutable(first, storage));
        QVERIFY(rememberExecutable(second, storage));
        QCOMPARE(recentExecutables(storage), QStringList({second, first}));

        QVERIFY(removeRecentExecutable(first, storage));
        QCOMPARE(recentExecutables(storage), QStringList({second}));
        QVERIFY(!removeRecentExecutable("missing.exe", storage));

        QVERIFY(removeRecentExecutable(second, storage));
        QCOMPARE(recentExecutables(storage), QStringList());
        QVERIFY(!QFile::exists(storage));
    }

    void recentItemContextMenuProvidesShortcutShowFilesAndRemove() {
        QTemporaryDir tmp;
        const QString storage = tmp.filePath("recent.json");
        const QString app1 = tmp.filePath("App1.exe");
        const QString app2 = tmp.filePath("App2.exe");
        for (const QString &path : {app1, app2}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("MZ");
            file.close();
        }
        QVERIFY(rememberExecutable(app1, storage));
        QVERIFY(rememberExecutable(app2, storage));

        FileUrlRecorder recorder;
        QDesktopServices::setUrlHandler("file", &recorder, "record");

        QTimer::singleShot(0, this, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto *list = dialog->findChild<QListWidget *>("recentList");
            QVERIFY(list);
            QCOMPARE(list->count(), 2);

            // Trigger context menu on first item (app2)
            const QPoint pos = list->visualItemRect(list->item(0)).center();
            QContextMenuEvent event(QContextMenuEvent::Mouse, pos, list->viewport()->mapToGlobal(pos));
            QApplication::sendEvent(list->viewport(), &event);

            auto *menu = dialog->findChild<QMenu *>("recentContextMenu");
            QVERIFY(menu);

            auto *shortcutMenu = menu->findChild<QMenu *>("recentMakeShortcutMenu");
            QVERIFY(shortcutMenu);
            QCOMPARE(shortcutMenu->title(), QString("Make Shortcut"));

            auto *desktopAction = shortcutMenu->findChild<QAction *>("recentDesktopAction");
            auto *startMenuAction = shortcutMenu->findChild<QAction *>("recentStartMenuAction");
            QVERIFY(desktopAction);
            QVERIFY(startMenuAction);
            QCOMPARE(desktopAction->text(), QString("Desktop"));
            QCOMPARE(startMenuAction->text(), QString("Start Menu"));

            auto *showFilesAction = menu->findChild<QAction *>("recentShowFilesAction");
            QVERIFY(showFilesAction);
            QCOMPARE(showFilesAction->text(), QString("Show Files"));

            auto *removeAction = menu->findChild<QAction *>("recentRemoveAction");
            QVERIFY(removeAction);
            QCOMPARE(removeAction->text(), QString("Remove From List"));

            // Test Show Files
            showFilesAction->trigger();
            QCOMPARE(recorder.opened.toLocalFile(), QFileInfo(app2).dir().absolutePath());

            // Test Remove From List
            removeAction->trigger();
            menu->close();

            QCOMPARE(list->count(), 1);
            QCOMPARE(recentExecutables(storage), QStringList({app1}));

            dialog->reject();
        });

        showExecutableOpener("/usr/bin/winbridge", nullptr, "", "", storage);
        QDesktopServices::unsetUrlHandler("file");
    }

    void appLoadingDialogDisplaysHeaderLabelAndIndeterminateBar() {
        AppLoadingDialog dialog("/home/user/games/Warcraft3.exe", "classic");
        QCOMPARE(dialog.objectName(), QString("appLoadingDialog"));
        QCOMPARE(dialog.windowTitle(), QString("WinBridge"));
        QVERIFY(!dialog.windowIcon().isNull());
        QCOMPARE(dialog.size(), QSize(500, 165));

        auto *topNav = dialog.findChild<QFrame *>("topNav");
        QVERIFY(topNav);
        auto *logo = topNav->findChild<QLabel *>("logo");
        QVERIFY(logo);
        auto *brand = topNav->findChild<QLabel *>("brand");
        QVERIFY(brand);
        QCOMPARE(brand->text(), QString("WinBridge"));
        auto *brandSub = topNav->findChild<QLabel *>("brandSub");
        QVERIFY(brandSub);
        QCOMPARE(brandSub->text(), QString("EXE APP LAUNCHER"));

        auto *loadingLabel = dialog.findChild<QLabel *>("loadingLabel");
        QVERIFY(loadingLabel);
        QCOMPARE(loadingLabel->text(), QString("Loading windows app Warcraft3.exe"));

        auto *bar = dialog.findChild<QProgressBar *>("loadingBar");
        QVERIFY(bar);
        QCOMPARE(bar->minimum(), 0);
        QCOMPARE(bar->maximum(), 0);
        QVERIFY(!bar->isTextVisible());
    }

    void matchingProcessPidsAndIsExecutableStartedDetectProcessLifecycle() {
        QTemporaryDir tmp;
        const QString procRoot = tmp.path();

        auto makeProc = [&](const QString &pidStr, const QByteArray &cmdline) {
            QDir(procRoot).mkdir(pidStr);
            QFile file(QDir(procRoot).filePath(pidStr + "/cmdline"));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(cmdline);
            file.close();
        };

        // Launcher process should NOT match
        QByteArray launcherCmd;
        launcherCmd.append("/usr/bin/winbridge").append('\0').append("--").append('\0').append("/games/doom.exe").append('\0');
        makeProc("2001", launcherCmd);

        // UMU run process should NOT match
        QByteArray umuCmd;
        umuCmd.append("python3").append('\0').append("/usr/bin/umu-run").append('\0').append("/games/doom.exe").append('\0');
        makeProc("2002", umuCmd);

        // Wineserver should NOT match
        QByteArray wineServerCmd;
        wineServerCmd.append("C:\\windows\\system32\\wineserver").append('\0');
        makeProc("2003", wineServerCmd);

        // Wine process running doom.exe should MATCH
        QByteArray wineCmd;
        wineCmd.append("C:\\games\\Doom.EXE").append('\0');
        makeProc("2004", wineCmd);

        // Wine preloader running doom.exe should MATCH
        QByteArray winePreloaderCmd;
        winePreloaderCmd.append("wine64-preloader").append('\0').append("Z:\\games\\doom.exe").append('\0');
        makeProc("2005", winePreloaderCmd);

        // Check matchingProcessPids
        const QSet<qint64> pids = matchingProcessPids("/games/doom.exe", "", procRoot);
        QCOMPARE(pids, QSet<qint64>({2004, 2005}));

        // Check isExecutableStarted when already recorded
        QVERIFY(!isExecutableStarted("/games/doom.exe", "", {2004, 2005}, procRoot));

        // When a new PID appears:
        QByteArray newInstanceCmd;
        newInstanceCmd.append("doom.exe").append('\0');
        makeProc("2006", newInstanceCmd);

        QVERIFY(isExecutableStarted("/games/doom.exe", "", {2004, 2005}, procRoot));
    }

    void isExecutableWindowVisibleDetectsWindowLifecycle() {
        QTemporaryDir tmp;
        const QString procRoot = tmp.path();

        // 1. Process not started yet -> returns false
        QVERIFY(!isExecutableWindowVisible("/games/doom.exe", "", {}, {}, procRoot));

        // 2. Start process in mock proc
        QDir(procRoot).mkdir("3001");
        QFile file(QDir(procRoot).filePath("3001/cmdline"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray("doom.exe").append('\0'));
        file.close();

        // 3. activeWindowIds returns current window set
        const QSet<unsigned long> windowsBefore = activeWindowIds();

        // 4. When all existing windows are treated as initialWindows, no new doom.exe window exists
#ifdef WINBRIDGE_HAVE_X11
        Display *disp = XOpenDisplay(nullptr);
        if (disp) {
            QVERIFY(!isExecutableWindowVisible("/games/doom.exe", "", {}, windowsBefore, procRoot));
            XCloseDisplay(disp);
        }
#endif
    }
};

QTEST_MAIN(OpenerTests)
#include "test_opener.moc"
