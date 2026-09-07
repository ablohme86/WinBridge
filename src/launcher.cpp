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

#include "launcher.h"
#include "shared_space.h"
#include "shortcuts.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <iostream>
#include <stdexcept>

namespace WinBridge {

QMap<QString, QString> parseKeyValuePairs(const QString &filePath) {
    QMap<QString, QString> result;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return result;

    QString content = QString::fromUtf8(file.readAll());
    static QRegularExpression re(R"re("([^"\n]+)"\s*"((?:\\.|[^"\\])*)")re");
    QRegularExpressionMatchIterator it = re.globalMatch(content);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString k = match.captured(1);
        QString v = match.captured(2);
        v.replace(R"(\\)", "\\").replace(R"(\")", "\"");
        result[k] = v;
    }
    return result;
}

QStringList steamRoots() {
    QString home = QDir::homePath();
    QStringList candidates = {
        home + "/.local/share/Steam",
        home + "/.steam/steam",
        home + "/.steam/root",
        home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam",
        home + "/.var/app/com.valvesoftware.Steam/data/Steam"
    };

    QStringList roots;
    for (const QString &c : candidates) {
        QFileInfo fi(c);
        if (fi.isDir()) {
            QString canon = fi.canonicalFilePath();
            QString resolved = canon.isEmpty() ? QDir::cleanPath(fi.absoluteFilePath()) : canon;
            if (!roots.contains(resolved)) {
                roots.append(resolved);
            }
        }
    }
    return roots;
}

QStringList steamLibraries(const QStringList &roots) {
    QStringList result = roots;
    for (const QString &root : roots) {
        QFile file(root + "/steamapps/libraryfolders.vdf");
        if (!file.open(QIODevice::ReadOnly)) continue;
        QString content = QString::fromUtf8(file.readAll());
        static QRegularExpression re(R"re("path"\s*"((?:\\.|[^"\\])*)")re");
        QRegularExpressionMatchIterator it = re.globalMatch(content);
        while (it.hasNext()) {
            QString val = it.next().captured(1);
            val.replace(R"(\\)", "\\").replace(R"(\")", "\"");
            QFileInfo fi(val);
            if (fi.isDir()) {
                QString canon = fi.canonicalFilePath();
                QString resolved = canon.isEmpty() ? QDir::cleanPath(fi.absoluteFilePath()) : canon;
                if (!result.contains(resolved)) {
                    result.append(resolved);
                }
            }
        }
    }
    return result;
}

QStringList discoverProtons(const QStringList &roots, const QStringList &libs) {
    QStringList folders;
    for (const QString &lib : libs) {
        folders << (lib + "/steamapps/common");
    }
    for (const QString &root : roots) {
        folders << (root + "/compatibilitytools.d");
    }
    folders << "/usr/share/steam/compatibilitytools.d";

    QString customEnv = qEnvironmentVariable("WINBRIDGE_SEARCH_PATHS");
    if (customEnv.isEmpty()) {
        customEnv = qEnvironmentVariable("PROTONRUN_SEARCH_PATHS");
    }
    if (!customEnv.isEmpty()) {
        for (const QString &p : customEnv.split(':', Qt::SkipEmptyParts)) {
            QString path = p.trimmed();
            if (path.startsWith("~/")) path = QDir::homePath() + path.mid(1);
            folders << path;
        }
    }

    QSet<QString> found;
    for (const QString &folder : folders) {
        QDir dir(folder);
        if (!dir.exists()) continue;

        if (QFile::exists(dir.filePath("proton"))) {
            QFileInfo fi(folder);
            QString canon = fi.canonicalFilePath();
            found.insert(canon.isEmpty() ? QDir::cleanPath(fi.absoluteFilePath()) : canon);
        }

        for (const QString &sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString subPath = dir.filePath(sub);
            if (QFile::exists(subPath + "/proton")) {
                QFileInfo fi(subPath);
                QString canon = fi.canonicalFilePath();
                found.insert(canon.isEmpty() ? QDir::cleanPath(fi.absoluteFilePath()) : canon);
            }
        }
    }

    QStringList list = found.values();
    std::sort(list.begin(), list.end(), [](const QString &a, const QString &b) {
        return QFileInfo(a).fileName().toLower() < QFileInfo(b).fileName().toLower();
    });
    return list;
}

QString runtimeFor(const QString &protonDir, const QStringList &libs) {
    auto manifest = parseKeyValuePairs(protonDir + "/toolmanifest.vdf");
    QString appid = manifest.value("require_tool_appid");
    if (appid.isEmpty() || appid == "0") return QString();

    for (const QString &lib : libs) {
        auto appManifest = parseKeyValuePairs(QString("%1/steamapps/appmanifest_%2.acf").arg(lib, appid));
        QString installDir = appManifest.value("installdir");
        if (!installDir.isEmpty()) {
            QString entryPoint = QString("%1/steamapps/common/%2/_v2-entry-point").arg(lib, installDir);
            if (QFile::exists(entryPoint)) {
                return entryPoint;
            }
        }
    }

    throw std::runtime_error(QString("%1 trenger Steam Linux Runtime (Steam App ID %2). Installer den fra Verktøy i Steam og prøv igjen.")
        .arg(QFileInfo(protonDir).fileName(), appid).toStdString());
}

QString dialogTool() {
    QString currentDesktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();
    bool kde = currentDesktop.contains("kde");
    QStringList order = kde ? QStringList{"kdialog", "zenity"} : QStringList{"zenity", "kdialog"};
    for (const QString &tool : order) {
        if (!QStandardPaths::findExecutable(tool).isEmpty()) {
            return tool;
        }
    }
    return QString();
}

void showError(const QString &message) {
    std::cerr << message.toLocal8Bit().constData() << std::endl;
    QString tool = dialogTool();
    if (tool == "kdialog") {
        QProcess::execute("kdialog", {"--title", "WinBridge", "--error", message});
    } else if (tool == "zenity") {
        QProcess::execute("zenity", {"--error", "--no-markup", "--title=WinBridge", "--text=" + message});
    }
}

QString chooseProton(const QStringList &versions, const QString &exeName) {
    QString tool = dialogTool();
    QString prompt = "Hvilken Proton-versjon vil du bruke i WinBridge?\nValget huskes for alle programmer.";
    if (tool.isEmpty()) {
        if (versions.isEmpty()) return QString();
        return versions.first();
    }

    QProcess proc;
    if (tool == "kdialog") {
        QStringList args = {"--title", "WinBridge", "--ok-label", "Open", "--cancel-label", "Avbryt", "--menu", prompt};
        for (int i = 0; i < versions.size(); ++i) {
            args << QString::number(i) << QString("%1 — %2").arg(QFileInfo(versions[i]).fileName(), versions[i]);
        }
        proc.start("kdialog", args);
    } else {
        QStringList args = {
            "--list", "--title=WinBridge", "--text=" + prompt, "--no-markup",
            "--ok-label=Open", "--cancel-label=Avbryt", "--width=820", "--height=380",
            "--column=ID", "--column=Proton-versjon", "--column=Plassering",
            "--hide-column=1", "--print-column=1"
        };
        for (int i = 0; i < versions.size(); ++i) {
            args << QString::number(i) << QFileInfo(versions[i]).fileName() << versions[i];
        }
        proc.start("zenity", args);
    }

    proc.waitForFinished(-1);
    if (proc.exitCode() != 0) return QString();
    QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (out.isEmpty()) return QString();
    bool ok = false;
    int idx = out.toInt(&ok);
    if (ok && idx >= 0 && idx < versions.size()) {
        return versions[idx];
    }
    return QString();
}

int launch(
    const QString &exe,
    const QString &proton,
    const QStringList &roots,
    const QStringList &libs,
    const QStringList &extra,
    const QString &prefixArg,
    bool capture,
    const QString &verb,
    QString *capturedOutput,
    const QString &launcherArg
) {
    QString runtime = runtimeFor(proton, libs);
    QString data = dataDir();

    QFileInfo exeFi(exe);
    QByteArray parentBytes = exeFi.dir().canonicalPath().toUtf8();
    QString key = QString::fromUtf8(QCryptographicHash::hash(parentBytes, QCryptographicHash::Sha256).toHex()).left(20);

    QString compat = prefixArg.isEmpty() ? sharedPrefix() : prefixArg;
    if (compat.startsWith("~/")) compat = QDir::homePath() + compat.mid(1);
    QDir(compat).mkpath(".");
    QFileInfo compatFi(compat);
    compat = compatFi.canonicalFilePath().isEmpty() ? QDir::cleanPath(compatFi.absoluteFilePath()) : compatFi.canonicalFilePath();

    QString stateHome = qEnvironmentVariable("XDG_STATE_HOME");
    if (stateHome.isEmpty()) stateHome = QDir::homePath() + "/.local/state";
    QString logsDir = stateHome + "/winbridge/logs";
    QDir(logsDir).mkpath(".");

    qint64 nowNs = QDateTime::currentMSecsSinceEpoch() * 1000000ULL;
    QString logfilePath = QString("%1/%2-%3.log").arg(logsDir, key, QString::number(nowNs));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("STEAM_COMPAT_DATA_PATH", compat);
    env.insert("STEAM_COMPAT_CLIENT_INSTALL_PATH", roots.isEmpty() ? (data + "/steam") : roots.first());
    env.insert("STEAM_COMPAT_INSTALL_PATH", exeFi.dir().absolutePath());
    env.insert("STEAM_COMPAT_LIBRARY_PATHS", libs.join(":"));
    env.insert("STEAM_COMPAT_APP_ID", "0");
    env.insert("SteamAppId", "0");
    env.insert("SteamGameId", "0");

    QString toolPaths = proton;
    if (!runtime.isEmpty()) {
        toolPaths += ":" + QFileInfo(runtime).dir().absolutePath();
    }
    env.insert("STEAM_COMPAT_TOOL_PATHS", toolPaths);

    QStringList targetArgs;
    if (exeFi.suffix().toLower() == "lnk") {
        targetArgs << "start.exe" << "/unix" << exe;
    } else {
        targetArgs << exe;
    }

    QString program;
    QStringList commandArgs;
    if (!runtime.isEmpty()) {
        program = runtime;
        commandArgs << "--verb=run" << "--" << (proton + "/proton") << verb;
        commandArgs.append(targetArgs);
        commandArgs.append(extra);
    } else {
        program = proton + "/proton";
        commandArgs << verb;
        commandArgs.append(targetArgs);
        commandArgs.append(extra);
    }

    QString effLauncher = launcherArg;
    if (effLauncher.isEmpty()) {
        effLauncher = data + "/winbridge";
        if (!QFile::exists(effLauncher)) {
            effLauncher = "/usr/local/bin/winbridge";
        }
    }

    QFile logFile(logfilePath);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        throw std::runtime_error(QString("Kunne ikke opprette loggfil: %1").arg(logfilePath).toStdString());
    }

    QString header = QString("Proton: %1\nExecutable: %2\nPrefix: %3\n").arg(proton, exe, compat);
    logFile.write(header.toUtf8());
    logFile.flush();

    QProcess proc;
    proc.setProcessEnvironment(env);
    proc.setWorkingDirectory(exeFi.dir().absolutePath());
    proc.setProgram(program);
    proc.setArguments(commandArgs);
    proc.setStandardOutputFile(logfilePath, QIODevice::Append);
    proc.setStandardErrorFile(logfilePath, QIODevice::Append);

    proc.start();
    if (!proc.waitForStarted(5000)) {
        logFile.close();
        throw std::runtime_error(QString("Kunne ikke starte prosess: %1").arg(program).toStdString());
    }

    QString desk = desktopDir();
    while (proc.state() == QProcess::Running) {
        importShortcuts(compat, proton, effLauncher, "", desk);
        if (proc.waitForFinished(3000)) {
            break;
        }
    }
    importShortcuts(compat, proton, effLauncher, "", desk);

    logFile.close();

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (!capture) {
            throw std::runtime_error(QString("Programmet avsluttet med feilkode %1.\nLogg: %2")
                .arg(proc.exitCode()).arg(logfilePath).toStdString());
        }
    }

    if (capture && capturedOutput) {
        QFile readLog(logfilePath);
        if (readLog.open(QIODevice::ReadOnly)) {
            *capturedOutput = QString::fromUtf8(readLog.readAll());
        }
    }

    return proc.exitCode();
}

} // namespace WinBridge
