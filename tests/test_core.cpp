/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://gnu.org>.
*/

#include <QtTest>
#include <QImage>
#include "shared_space.h"
#include "shortcuts.h"
#include "manager_backend.h"
#include "launcher.h"

using namespace WinBridge;

class CoreTests : public QObject {
    Q_OBJECT

private slots:
    void testDesktopQuoteAndField() {
        QCOMPARE(desktopQuote("100% game"), QString("\"100%% game\""));
        QVERIFY(desktopQuote("$HOME").contains("\\\\$"));
        QCOMPARE(field("line1\nline2\r\\test"), QString("line1\\nline2\\r\\\\test"));
    }

    void testShortcutTargetRejection() {
        QTemporaryDir tmp;
        QString pfx = tmp.path();
        QVERIFY(shortcutTarget("sh -c \"touch /tmp/bad\"", pfx).isEmpty());
        QVERIFY(shortcutTarget(desktopQuote(R"(C:\..\..\outside.exe)"), pfx).isEmpty());
    }

    void testSettingsReadSaveAndLock() {
        QTemporaryDir tmp;
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
        qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());

        QJsonObject init = readSettings();
        QVERIFY(init.isEmpty());

        QJsonObject s;
        s["proton"] = "/path/to/proton";
        s["prefix"] = "/path/to/prefix";
        QVERIFY(saveSettings(s));

        QJsonObject loaded = readSettings();
        QCOMPARE(loaded["proton"].toString(), QString("/path/to/proton"));
        QCOMPARE(loaded["prefix"].toString(), QString("/path/to/prefix"));

        {
            SettingsLock lock;
            QVERIFY(lock.isLocked());
        }
    }

    void testLegacySettingsAdoption() {
        QTemporaryDir tmp;
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());
        qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());

        QString legacyDir = tmp.path() + "/protonrun";
        QDir().mkpath(legacyDir);
        QFile legFile(legacyDir + "/settings.json");
        QVERIFY(legFile.open(QIODevice::WriteOnly));
        legFile.write(R"({"prefix": "/legacy/prefix", "proton": "/legacy/proton"})");
        legFile.close();

        QJsonObject loaded = readSettings();
        QCOMPARE(loaded["prefix"].toString(), QString("/legacy/prefix"));
        QCOMPARE(loaded["proton"].toString(), QString("/legacy/proton"));
    }

    void testParsePrograms() {
        QString output = "wine: warning\n{B}|||\xC3\x98velse\n{A}|||App\n{A}|||App\n";
        auto list = parsePrograms(output);
        QCOMPARE(list.size(), 2);
        QCOMPARE(list[0].first, QString("{A}"));
        QCOMPARE(list[0].second, QString("App"));
        QCOMPARE(list[1].first, QString("{B}"));
        QCOMPARE(list[1].second, QString::fromUtf8("Øvelse"));
    }

    void testRunningAppsAndKill() {
        QTemporaryDir tmp;
        QString pfx = tmp.filePath("prefix");
        QString procRoot = tmp.filePath("proc");
        QDir().mkpath(procRoot + "/1000");
        QDir().mkpath(procRoot + "/1001");

        QFile s1000(procRoot + "/1000/status");
        QVERIFY(s1000.open(QIODevice::WriteOnly));
        s1000.write("PPid:\t1\n");
        s1000.close();

        QFile e1000(procRoot + "/1000/environ");
        QVERIFY(e1000.open(QIODevice::WriteOnly));
        QByteArray envData = QString("STEAM_COMPAT_DATA_PATH=%1").arg(pfx).toUtf8();
        envData.append('\0');
        e1000.write(envData);
        e1000.close();

        QFile c1000(procRoot + "/1000/cmdline");
        QVERIFY(c1000.open(QIODevice::WriteOnly));
        QByteArray cmdData = "C:\\Games\\SimCity\\sc3u.exe";
        cmdData.append('\0');
        c1000.write(cmdData);
        c1000.close();

        // 1001 is child of 1000
        QFile s1001(procRoot + "/1001/status");
        QVERIFY(s1001.open(QIODevice::WriteOnly));
        s1001.write("PPid:\t1000\n");
        s1001.close();

        QFile e1001(procRoot + "/1001/environ");
        QVERIFY(e1001.open(QIODevice::WriteOnly));
        e1001.write(envData);
        e1001.close();

        QFile c1001(procRoot + "/1001/cmdline");
        QVERIFY(c1001.open(QIODevice::WriteOnly));
        QByteArray cmdData2 = "helper.exe";
        cmdData2.append('\0');
        c1001.write(cmdData2);
        c1001.close();

        QJsonArray progs;
        QJsonObject progObj;
        progObj["key"] = "sc_key";
        progObj["name"] = "SimCity";
        progObj["shortcuts"] = QJsonArray();
        progs.append(progObj);

        auto running = getRunningApps(pfx, progs, procRoot);
        QVERIFY(running.contains("sc_key"));
        QCOMPARE(running["sc_key"].size(), 2);
        QCOMPARE(running["sc_key"][0], qint64(1000));
        QCOMPARE(running["sc_key"][1], qint64(1001));
    }

    void testShortcutsConfigAndToggle() {
        QTemporaryDir tmp;
        qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());

        QJsonObject empty = readShortcutsConfig();
        QVERIFY(empty.isEmpty());

        QJsonObject conf;
        QJsonObject s1;
        s1["desktop"] = false;
        s1["menu"] = true;
        conf["sc1"] = s1;
        QVERIFY(saveShortcutsConfig(conf));

        QJsonObject readBack = readShortcutsConfig();
        QCOMPARE(readBack["sc1"].toObject()["desktop"].toBool(), false);
        QCOMPARE(readBack["sc1"].toObject()["menu"].toBool(), true);

        QJsonObject res = toggleShortcut(tmp.path(), "sc1", true, false, tmp.filePath("launcher"), "dummy_proton", tmp.filePath("data"), tmp.filePath("desktop"));
        QCOMPARE(res["desktop"].toBool(), true);
        QCOMPARE(res["menu"].toBool(), false);
    }

    void testImportShortcutsAndIconGeneration() {
        QTemporaryDir tmp;
        QString pfx = tmp.path();
        QString sourceDir = pfx + "/pfx/drive_c/proton_shortcuts";
        QDir().mkpath(sourceDir + "/icons/48x48/apps");

        // Create a 48x48 test png icon
        QImage testImg(48, 48, QImage::Format_ARGB32);
        testImg.fill(Qt::blue);
        QVERIFY(testImg.save(sourceDir + "/icons/48x48/apps/game_icon.0.png"));

        // Create a test .desktop file in proton_shortcuts
        QDir().mkpath(pfx + "/pfx/drive_c/Games/TestGame");
        QFile dummyExe(pfx + "/pfx/drive_c/Games/TestGame/game.exe");
        QVERIFY(dummyExe.open(QIODevice::WriteOnly));
        dummyExe.write("MZ");
        dummyExe.close();

        QFile scFile(sourceDir + "/TestGame.desktop");
        QVERIFY(scFile.open(QIODevice::WriteOnly));
        scFile.write(QString("[Desktop Entry]\nType=Application\nName=Test Game\nExec=%1\nIcon=game_icon.0\n")
            .arg(desktopQuote(R"(C:\Games\TestGame\game.exe)")).toUtf8());
        scFile.close();

        QString customData = tmp.filePath("data");
        QString customDesktop = tmp.filePath("desktop");
        QDir().mkpath(customData + "/applications");
        QDir().mkpath(customDesktop);

        QStringList imported = importShortcuts(pfx, "dummy_proton", tmp.filePath("winbridge"), customData, customDesktop);
        QCOMPARE(imported.size(), 1);
        QCOMPARE(imported[0], QString("Test Game"));

        // Verify .desktop was created in customData/applications and customDesktop
        QDir appDir(customData + "/applications");
        QStringList appFiles = appDir.entryList({"winbridge-*.desktop"}, QDir::Files);
        QCOMPARE(appFiles.size(), 1);

        QDir deskDir(customDesktop);
        QStringList deskFiles = deskDir.entryList({"winbridge-*.desktop"}, QDir::Files);
        QCOMPARE(deskFiles.size(), 1);

        // Read the created .desktop file and check Icon
        QFile created(appDir.filePath(appFiles[0]));
        QVERIFY(created.open(QIODevice::ReadOnly));
        QString content = QString::fromUtf8(created.readAll());
        created.close();

        // Icon should be winbridge-<identity>
        QString identity = appFiles[0].mid(10, appFiles[0].length() - 18);
        QVERIFY(content.contains("Icon=winbridge-" + identity));

        // Verify hicolor icons were generated for standard sizes, e.g. 48x48 and 16x16
        QString hicolor48 = QString("%1/icons/hicolor/48x48/apps/winbridge-%2.png").arg(customData, identity);
        QString hicolor16 = QString("%1/icons/hicolor/16x16/apps/winbridge-%2.png").arg(customData, identity);
        QVERIFY(QFile::exists(hicolor48));
        QVERIFY(QFile::exists(hicolor16));

        // Verify the scaled 16x16 icon has correct dimensions
        QImage readImg(hicolor16);
        QCOMPARE(readImg.width(), 16);
        QCOMPARE(readImg.height(), 16);
    }

    void testRemoveProgramShortcutsAndCleanup() {
        QTemporaryDir tmp;
        QString pfx = tmp.path();
        QString sourceDir = pfx + "/pfx/drive_c/proton_shortcuts";
        QDir().mkpath(sourceDir + "/icons/48x48/apps");

        QImage testImg(48, 48, QImage::Format_ARGB32);
        testImg.fill(Qt::red);
        QVERIFY(testImg.save(sourceDir + "/icons/48x48/apps/app_icon.0.png"));
        // Create an orphaned uninstaller icon
        QVERIFY(testImg.save(sourceDir + "/icons/48x48/apps/unins000.0.png"));

        QDir().mkpath(pfx + "/pfx/drive_c/Games/DemoApp");
        QFile exeFile(pfx + "/pfx/drive_c/Games/DemoApp/demo.exe");
        QVERIFY(exeFile.open(QIODevice::WriteOnly));
        exeFile.write("MZ");
        exeFile.close();

        QFile scFile(sourceDir + "/DemoApp.desktop");
        QVERIFY(scFile.open(QIODevice::WriteOnly));
        scFile.write(QString("[Desktop Entry]\nType=Application\nName=Demo App\nExec=%1\nIcon=app_icon.0\n")
            .arg(desktopQuote(R"(C:\Games\DemoApp\demo.exe)")).toUtf8());
        scFile.close();

        QString customData = tmp.filePath("data");
        QString customDesktop = tmp.filePath("desktop");
        QDir().mkpath(customData + "/applications");
        QDir().mkpath(customDesktop);

        importShortcuts(pfx, "dummy", tmp.filePath("winbridge"), customData, customDesktop);

        QVERIFY(QFile::exists(sourceDir + "/DemoApp.desktop"));
        QVERIFY(QFile::exists(sourceDir + "/icons/48x48/apps/app_icon.0.png"));
        QVERIFY(QFile::exists(sourceDir + "/icons/48x48/apps/unins000.0.png"));

        // Simulate program uninstall
        ProgramRegistryMeta meta;
        meta.key = "demo_key";
        meta.name = "Demo App";
        meta.loc = R"(C:\Games\DemoApp)";
        removeProgramShortcuts(pfx, "demo_key", "Demo App", meta, customData, customDesktop);
        cleanupOrphanedShortcuts(pfx, customData, customDesktop);

        // Verify .desktop in proton_shortcuts is gone
        QVERIFY(!QFile::exists(sourceDir + "/DemoApp.desktop"));
        // Verify app icon in proton_shortcuts is gone
        QVERIFY(!QFile::exists(sourceDir + "/icons/48x48/apps/app_icon.0.png"));
        // Verify orphaned uninstaller icon in proton_shortcuts is also gone
        QVERIFY(!QFile::exists(sourceDir + "/icons/48x48/apps/unins000.0.png"));
        // Verify Linux .desktop in applications and desktop are gone
        QDir appDir(customData + "/applications");
        QCOMPARE(appDir.entryList({"winbridge-*.desktop"}, QDir::Files).size(), 0);
        QDir deskDir(customDesktop);
        QCOMPARE(deskDir.entryList({"winbridge-*.desktop"}, QDir::Files).size(), 0);
    }
};

QTEST_MAIN(CoreTests)
#include "test_core.moc"
