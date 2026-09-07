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

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <iostream>

using namespace WinBridge;

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("WinBridge");
    app.setApplicationVersion("0.2.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Open Windows executables using an installed Proton, with a desktop chooser.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption importShortcutsOpt("import-shortcuts", "Import existing shortcuts; requires --prefix and --proton");
    parser.addOption(importShortcutsOpt);

    QCommandLineOption configureOpt("configure", "Change the shared Proton version");
    parser.addOption(configureOpt);

    QCommandLineOption listOpt("list", "List installed Proton versions");
    parser.addOption(listOpt);

    QCommandLineOption protonOpt("proton", "Use this Proton directory without a chooser", "path");
    parser.addOption(protonOpt);

    QCommandLineOption prefixOpt("prefix", "Use a specific compatdata directory", "path");
    parser.addOption(prefixOpt);

    parser.addPositionalArgument("exe", "Windows executable (.exe or .lnk) to run", "[exe]");
    parser.addPositionalArgument("arguments", "Arguments passed to the executable", "[arguments...]");

    parser.process(app);

    QStringList roots = steamRoots();
    QStringList libs = steamLibraries(roots);
    QStringList versions = discoverProtons(roots, libs);

    if (parser.isSet(listOpt)) {
        for (const QString &v : versions) {
            std::cout << QFileInfo(v).fileName().toUtf8().constData() << "\t" << v.toUtf8().constData() << "\n";
        }
        return 0;
    }

    try {
        if (parser.isSet(configureOpt)) {
            selectProton(
                versions,
                [&](const QStringList &items) { return chooseProton(items, "WinBridge"); },
                [&](const QString &p) { runtimeFor(p, libs); },
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
            QString tool = dialogTool();
            QString home = QDir::homePath();
            QProcess dialogProc;
            if (tool == "kdialog") {
                dialogProc.start("kdialog", {"--getopenfilename", home, "*.exe *.EXE|Windows-programmer"});
            } else {
                dialogProc.start("zenity", {"--file-selection", "--title=Velg en .exe-fil", "--file-filter=Windows | *.exe *.EXE"});
            }
            dialogProc.waitForFinished(-1);
            if (dialogProc.exitCode() != 0) {
                return 0;
            }
            exePath = QString::fromUtf8(dialogProc.readAllStandardOutput()).trimmed();
            if (exePath.isEmpty()) return 0;
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
            if (!QFile::exists(prtFi.filePath() + "/proton")) {
                throw std::runtime_error("Denne mappen inneholder ikke Proton.");
            }
            chosenProton = prtFi.canonicalFilePath().isEmpty() ? QDir::cleanPath(prtFi.absoluteFilePath()) : prtFi.canonicalFilePath();
        } else {
            chosenProton = selectProton(
                versions,
                [&](const QStringList &items) { return chooseProton(items, exeFi.fileName()); },
                [&](const QString &p) { runtimeFor(p, libs); }
            );
        }

        if (chosenProton.isEmpty()) return 0;

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
