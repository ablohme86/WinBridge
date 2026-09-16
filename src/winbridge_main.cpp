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

#include "shared_space.h"
#include "shortcuts.h"
#include "launcher.h"
#include "opener.h"
#include "single_instance.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <iostream>

using namespace WinBridge;

static int runWinBridge(QCoreApplication &app, bool showOpener) {
    app.setApplicationName("WinBridge");
    app.setApplicationVersion("0.2.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Open Windows executables using Proton and UMU, without requiring Steam.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption importShortcutsOpt("import-shortcuts", "Import existing shortcuts; requires --prefix and --proton");
    parser.addOption(importShortcutsOpt);

    QCommandLineOption configureOpt("configure", "Change the shared Proton version");
    parser.addOption(configureOpt);

    QCommandLineOption listOpt("list", "List installed Proton versions");
    parser.addOption(listOpt);

    QCommandLineOption protonOpt("proton", "Proton directory, GE-Proton, or UMU-Proton", "path");
    parser.addOption(protonOpt);
    QCommandLineOption installOpt("install-proton", "Download Proton and its runtime (GE-Proton or UMU-Proton)", "version");
    parser.addOption(installOpt);

    QCommandLineOption prefixOpt("prefix", "Use a specific compatdata directory", "path");
    parser.addOption(prefixOpt);

    QCommandLineOption screenshotOpt("screenshot", "Render the app opener to an image and exit", "path");
    parser.addOption(screenshotOpt);
    QCommandLineOption themeOpt("theme", "App opener theme: classic or dark", "theme");
    parser.addOption(themeOpt);

    parser.addPositionalArgument("exe", "Windows executable (.exe or .lnk) to run", "[exe]");
    parser.addPositionalArgument("arguments", "Arguments passed to the executable", "[arguments...]");

    parser.process(app);

    QStringList roots = steamRoots();
    QStringList libs = steamLibraries(roots);
    QStringList versions = protonChoices(roots, libs);

    if (parser.isSet(listOpt)) {
        for (const QString &v : discoverProtons(roots, libs)) {
            std::cout << QFileInfo(v).fileName().toUtf8().constData() << "\t" << v.toUtf8().constData() << "\n";
        }
        return 0;
    }

    try {
        if (parser.isSet(installOpt)) {
            installProton(parser.value(installOpt));
            std::cout << "Proton og runtime er klare. Velg versjon med winbridge --configure.\n";
            return 0;
        }
        if (parser.isSet(configureOpt)) {
            selectProton(
                versions,
                [&](const QStringList &items) { return chooseProton(items, "WinBridge"); },
                [&](const QString &p) { validateProton(p, libs); },
                true
            );
            return 0;
        }

        if (parser.isSet(importShortcutsOpt)) {
            QString pfx = parser.value(prefixOpt);
            QString prt = parser.value(protonOpt);
            if (pfx.isEmpty() || prt.isEmpty()) {
                throw std::runtime_error("--import-shortcuts krever --prefix og --proton.");
            }
            QStringList imported = importShortcuts(
                QDir::cleanPath(pfx),
                QDir::cleanPath(prt),
                QCoreApplication::applicationFilePath(),
                "",
                desktopDir()
            );
            std::cout << "Importert: " << imported.join(", ").toUtf8().constData() << "\n";
            return 0;
        }

        const QStringList positional = parser.positionalArguments();
        QString exePath;
        QStringList extraArgs;

        if (positional.isEmpty()) {
            if (!showOpener) return 0;
            OpenRequest request = showExecutableOpener(
                QCoreApplication::applicationFilePath(), nullptr,
                parser.value(screenshotOpt), parser.value(themeOpt));
            if (parser.isSet(screenshotOpt)) return request.screenshotSaved ? 0 : 1;
            if (!request.accepted) return 0;
            exePath = request.executable;
        } else {
            exePath = positional.first();
            extraArgs = positional.mid(1);
        }

        if (exePath.startsWith("~/")) {
            exePath = QDir::homePath() + exePath.mid(1);
        }
        QFileInfo exeFi(exePath);
        if (!exeFi.isFile() || (exeFi.suffix().toLower() != "exe" && exeFi.suffix().toLower() != "lnk")) {
            throw std::runtime_error("Velg en eksisterende .exe-fil.");
        }
        exePath = exeFi.canonicalFilePath().isEmpty() ? QDir::cleanPath(exeFi.absoluteFilePath()) : exeFi.canonicalFilePath();
        QString chosenProton;
        if (parser.isSet(protonOpt)) {
            QString p = parser.value(protonOpt);
            if (p.startsWith("~/")) p = QDir::homePath() + p.mid(1);
            QFileInfo prtFi(p);
            if (!isProtonAvailable(p)) {
                throw std::runtime_error("Denne mappen inneholder ikke Proton.");
            }
            chosenProton = isProtonDownload(p) ? p : (prtFi.canonicalFilePath().isEmpty() ? QDir::cleanPath(prtFi.absoluteFilePath()) : prtFi.canonicalFilePath());
        } else {
            chosenProton = selectProton(
                versions,
                [&](const QStringList &items) { return chooseProton(items, exeFi.fileName()); },
                [&](const QString &p) { validateProton(p, libs); }
            );
        }

        if (chosenProton.isEmpty()) return 0;
        rememberExecutable(exePath);

        return launch(
            exePath,
            chosenProton,
            roots,
            libs,
            extraArgs,
            parser.value(prefixOpt),
            false,
            "run",
            nullptr,
            QCoreApplication::applicationFilePath()
        );
    } catch (const std::exception &exc) {
        showError(QString::fromUtf8(exc.what()));
        return 1;
    }
}

int main(int argc, char **argv) {
    // Use Widgets only when no executable or command action was supplied. This
    // keeps terminal commands and file-manager launches independent of a GUI.
    bool graphicalOpener = true;
    const QStringList commandActions = {
        "--help", "-h", "--version", "-v", "--list", "--configure",
        "--import-shortcuts", "--install-proton"
    };
    bool consumeValue = false;
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (consumeValue) {
            consumeValue = false;
            continue;
        }
        if (commandActions.contains(argument) || argument.startsWith("--install-proton=")) {
            graphicalOpener = false;
            break;
        }
        if (argument == "--proton" || argument == "--prefix" ||
            argument == "--screenshot" || argument == "--theme") {
            consumeValue = true;
            continue;
        }
        if (argument.startsWith("--proton=") || argument.startsWith("--prefix=") ||
            argument.startsWith("--screenshot=") || argument.startsWith("--theme=")) continue;
        if (argument == "--") {
            if (i + 1 < argc) graphicalOpener = false;
            break;
        }
        if (!argument.startsWith('-')) {
            graphicalOpener = false;
            break;
        }
    }
    if (graphicalOpener) {
        QApplication app(argc, argv);
        app.setDesktopFileName("winbridge");
        bool screenshot = false;
        for (int i = 1; i < argc; ++i) {
            const QString argument = QString::fromLocal8Bit(argv[i]);
            if (argument == "--screenshot" || argument.startsWith("--screenshot=")) { screenshot = true; break; }
        }
        if (!screenshot && activateRunningInstance("launcher", qEnvironmentVariable("XDG_ACTIVATION_TOKEN"))) return 0;
        return runWinBridge(app, true);
    }
    QCoreApplication app(argc, argv);
    return runWinBridge(app, false);
}
