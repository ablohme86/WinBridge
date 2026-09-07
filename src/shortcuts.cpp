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

#include "shortcuts.h"
#include "shared_space.h"
#include <functional>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <sys/stat.h>

namespace WinBridge {

QString desktopQuote(const QString &value) {
    QString val = value;
    val.replace('%', "%%");
    val.replace('\\', "\\\\");
    val.replace('"', "\\\"");
    val.replace('`', "\\`");
    val.replace('$', "\\$");
    val.replace('\\', "\\\\");
    return "\"" + val + "\"";
}

QString field(const QString &value) {
    QString val = value;
    val.replace('\\', "\\\\");
    val.replace('\n', "\\n");
    val.replace('\r', "\\r");
    return val;
}

QString desktopDir() {
    QProcess proc;
    proc.start("xdg-user-dir", {"DESKTOP"});
    if (proc.waitForFinished(1000) && proc.exitCode() == 0) {
        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (!out.isEmpty() && QDir::isAbsolutePath(out) && out != QDir::homePath()) {
            return QDir::cleanPath(out);
        }
    }
    return QString();
}

QString enclosingPrefix(const QString &exePath) {
    QFileInfo fi(exePath);
    QDir dir = fi.dir();
    while (!dir.isRoot()) {
        if (dir.dirName() == "drive_c") {
            QDir parent = dir;
            if (parent.cdUp() && parent.dirName() == "pfx") {
                if (parent.cdUp()) {
                    QString canon = parent.canonicalPath();
                    return canon.isEmpty() ? QDir::cleanPath(parent.absolutePath()) : canon;
                }
            }
        }
        if (!dir.cdUp()) break;
    }
    return QString();
}

QString shortcutTarget(const QString &value, const QString &prefix) {
    QString unescaped;
    for (int i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            QChar c = value[i + 1];
            if (c == 's') { unescaped += ' '; i++; }
            else if (c == 'n') { unescaped += '\n'; i++; }
            else if (c == 't') { unescaped += '\t'; i++; }
            else if (c == 'r') { unescaped += '\r'; i++; }
            else if (c == '\\') { unescaped += '\\'; i++; }
            else { unescaped += value[i]; }
        } else {
            unescaped += value[i];
        }
    }

    QStringList tokens;
    QString cur;
    bool inSingle = false, inDouble = false, escaped = false;
    for (int i = 0; i < unescaped.size(); ++i) {
        QChar c = unescaped[i];
        if (escaped) {
            cur += c;
            escaped = false;
        } else if (c == '\\' && !inSingle) {
            escaped = true;
        } else if (c == '\'' && !inDouble) {
            inSingle = !inSingle;
        } else if (c == '"' && !inSingle) {
            inDouble = !inDouble;
        } else if (c.isSpace() && !inSingle && !inDouble) {
            if (!cur.isEmpty()) {
                tokens << cur;
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.isEmpty()) tokens << cur;
    if (tokens.size() != 1) return QString();
    QString token = tokens[0];

    static QRegularExpression driveRegex("^[cC]:[\\\\/]");
    if (!driveRegex.match(token).hasMatch()) return QString();

    QString rel = token.mid(3);
    rel.replace('\\', '/');

    QString cleanPrefix = QDir::cleanPath(prefix);
    QDir driveCDir(cleanPrefix + "/pfx/drive_c");
    QString driveC = driveCDir.canonicalPath();
    if (driveC.isEmpty()) driveC = QDir::cleanPath(driveCDir.absolutePath());

    QFileInfo targetFi(driveC + "/" + rel);
    QString targetPath = targetFi.canonicalFilePath();
    if (targetPath.isEmpty()) return QString();
    if (!targetPath.startsWith(driveC + "/") && targetPath != driveC) return QString();

    QString suffix = targetFi.suffix().toLower();
    if (suffix != "lnk" && suffix != "exe") return QString();
    if (!targetFi.isFile()) return QString();

    return targetPath;
}

QString shortcutsConfigPath() {
    QString conf = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (conf.isEmpty()) {
        conf = QDir::homePath() + "/.config";
    }
    return QDir::cleanPath(conf + "/winbridge/shortcuts.json");
}

QJsonObject readShortcutsConfig() {
    QFile file(shortcutsConfigPath());
    if (!file.open(QIODevice::ReadOnly)) return QJsonObject();
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject();
}

bool saveShortcutsConfig(const QJsonObject &config) {
    QString path = shortcutsConfigPath();
    QDir dir = QFileInfo(path).dir();
    if (!dir.exists()) dir.mkpath(".");

    QTemporaryFile temp(path + ".tmp.XXXXXX");
    if (!temp.open()) return false;
    temp.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
    temp.flush();
    QString tempPath = temp.fileName();
    temp.close();

    QFile::remove(path);
    return QFile::rename(tempPath, path);
}

QString shortcutIconPath(const QString &sourceDir, const QString &iconName) {
    if (iconName.isEmpty() || QFileInfo(iconName).fileName() != iconName) return QString();
    QDir iconsDir(sourceDir + "/icons");
    if (!iconsDir.exists()) return QString();

    QString cleanIcon = iconName.endsWith(".png", Qt::CaseInsensitive) ? iconName.left(iconName.length() - 4) : iconName;
    QString bestPath;
    int maxRes = -1;
    for (const QString &sub : iconsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString p = iconsDir.filePath(sub + "/apps/" + cleanIcon + ".png");
        QFileInfo fi(p);
        if (fi.isFile()) {
            int res = 0;
            if (sub.contains('x')) {
                res = sub.split('x')[0].toInt();
            }
            if (res >= maxRes) {
                maxRes = res;
                bestPath = fi.canonicalFilePath().isEmpty() ? QDir::cleanPath(fi.absoluteFilePath()) : fi.canonicalFilePath();
            }
        }
    }
    return bestPath;
}

static QMap<QString, QString> parseDesktopFile(const QString &path) {
    QMap<QString, QString> entry;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return entry;
    bool inDesktopEntry = false;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith('[')) {
            inDesktopEntry = (line == "[Desktop Entry]");
            continue;
        }
        if (!inDesktopEntry || line.startsWith('#') || !line.contains('=')) continue;
        int idx = line.indexOf('=');
        QString key = line.left(idx).trimmed();
        QString val = line.mid(idx + 1).trimmed();
        entry[key] = val;
    }
    return entry;
}

static bool isUninstallerShortcut(const QString &name, const QString &exec) {
    QString n = name.toLower();
    QString e = exec.toLower();
    if (n.contains("uninstall") || n.contains("avinstaller")) {
        return true;
    }
    if (e.contains("unins") || e.contains("unwise") || e.contains("isuninst")) {
        return true;
    }
    return false;
}

QList<ShortcutInfo> loadShortcuts(const QString &prefix, const QString &customDataDir, const QString &customDesktop) {
    QString pfx = QDir::cleanPath(prefix);
    QString data = customDataDir.isEmpty() ? (qEnvironmentVariable("XDG_DATA_HOME").isEmpty() ? QDir::homePath() + "/.local/share" : qEnvironmentVariable("XDG_DATA_HOME")) : customDataDir;
    QString desktop = customDesktop.isEmpty() ? desktopDir() : customDesktop;

    QString source = pfx + "/pfx/drive_c/proton_shortcuts";
    QDir srcDir(source);
    QJsonObject config = readShortcutsConfig();
    QList<ShortcutInfo> shortcuts;

    for (const QString &file : srcDir.entryList({"*.desktop"}, QDir::Files, QDir::Name)) {
        QString scPath = srcDir.filePath(file);
        auto entry = parseDesktopFile(scPath);
        QString execVal = entry.value("Exec");
        QString target = shortcutTarget(execVal, pfx);
        if (target.isEmpty()) continue;

        QString name = entry.value("Name", QFileInfo(file).completeBaseName());
        if (isUninstallerShortcut(name, execVal)) continue;

        QByteArray idData = pfx.toUtf8() + '\0' + file.toUtf8();
        QString identity = QString::fromUtf8(QCryptographicHash::hash(idData, QCryptographicHash::Sha256).toHex()).left(20);
        QString filename = QString("winbridge-%1.desktop").arg(identity);

        QString iconName = entry.value("Icon");
        QString hicolorPath = QString("%1/icons/hicolor/48x48/apps/winbridge-%2.png").arg(data, identity);
        QString icon;
        if (QFile::exists(hicolorPath)) {
            icon = hicolorPath;
        } else {
            icon = shortcutIconPath(source, iconName);
            if (icon.isEmpty()) {
                icon = QString("winbridge-%1").arg(identity);
            }
        }

        QJsonObject pref = config.value(identity).toObject();
        QString appFile = data + "/applications/" + filename;
        QString deskFile = desktop.isEmpty() ? QString() : desktop + "/" + filename;

        bool menuEnabled = pref.contains("menu") ? pref.value("menu").toBool() : (!config.isEmpty() ? QFile::exists(appFile) : true);
        bool desktopEnabled = pref.contains("desktop") ? pref.value("desktop").toBool() : (!config.isEmpty() ? (!desktop.isEmpty() && QFile::exists(deskFile)) : true);

        ShortcutInfo sc;
        sc.id = identity;
        sc.name = name;
        sc.file = file;
        sc.icon = icon;
        sc.path = entry.value("Path");
        sc.exec = execVal;
        sc.desktop = desktopEnabled;
        sc.menu = menuEnabled;
        shortcuts.append(sc);
    }
    return shortcuts;
}

static QString decodeRegString(const QString &input) {
    QString s = input;
    static QRegularExpression hexRegex(R"(\\x([0-9a-fA-F]{4}))");
    QRegularExpressionMatchIterator it = hexRegex.globalMatch(s);
    int offset = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        ushort unicodeVal = match.captured(1).toUShort(nullptr, 16);
        QString rep = QString(QChar(unicodeVal));
        s.replace(match.capturedStart() + offset, match.capturedLength(), rep);
        offset += rep.length() - match.capturedLength();
    }
    return s;
}

QMap<QString, ProgramRegistryMeta> getProgramRegistryMeta(const QString &prefix) {
    QString pfx = QDir::cleanPath(prefix);
    QMap<QString, ProgramRegistryMeta> meta;
    QStringList regFiles = {pfx + "/pfx/system.reg", pfx + "/pfx/user.reg"};

    for (const QString &regPath : regFiles) {
        QFile f(regPath);
        if (!f.open(QIODevice::ReadOnly)) continue;
        QString content = QString::fromUtf8(f.readAll());

        static QRegularExpression sectionRegex(R"(\[Software\\\\(?:Wow6432Node\\\\)?Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\([^\]]+)\](.*?)(?=\n\[|\Z))", QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator it = sectionRegex.globalMatch(content);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString key = match.captured(1).trimmed();
            QString body = match.captured(2);

            static QRegularExpression nameRe(R"(\"DisplayName\"=\"([^\"]+)\")");
            static QRegularExpression locRe(R"(\"(?:InstallLocation|Inno Setup: App Path)\"=\"([^\"]+)\")");
            static QRegularExpression grpRe(R"(\"Inno Setup: Icon Group\"=\"([^\"]+)\")");
            static QRegularExpression iconRe(R"(\"DisplayIcon\"=\"([^\"]+)\")");

            auto nameM = nameRe.match(body);
            auto locM = locRe.match(body);
            auto grpM = grpRe.match(body);
            auto iconM = iconRe.match(body);

            QString disp = nameM.hasMatch() ? decodeRegString(nameM.captured(1)) : "";
            QString loc = locM.hasMatch() ? locM.captured(1).replace(R"(\\)", "/").trimmed() : "";
            if (loc.startsWith('/')) loc = loc.mid(1);
            if (loc.startsWith("C:", Qt::CaseInsensitive)) loc = loc.mid(2).trimmed();
            if (loc.startsWith('/')) loc = loc.mid(1);

            QString grp = grpM.hasMatch() ? grpM.captured(1).replace(R"(\\)", "/") : "";
            QString ic = "";
            if (iconM.hasMatch()) {
                QString raw = iconM.captured(1).replace(R"(\\)", "/");
                ic = QFileInfo(raw).completeBaseName();
            }

            ProgramRegistryMeta m;
            m.key = key;
            m.name = disp;
            m.loc = loc;
            m.group = grp;
            m.icon = ic;
            meta[key.toLower()] = m;
        }
    }
    return meta;
}

QJsonArray groupShortcutsByProgram(const QString &prefix, const QJsonArray &programs) {
    auto meta = getProgramRegistryMeta(prefix);
    auto allShortcuts = loadShortcuts(prefix);
    QMap<QString, QJsonArray> grouped;

    for (const auto &progVal : programs) {
        grouped[progVal.toObject().value("key").toString()] = QJsonArray();
    }

    for (const auto &sc : allShortcuts) {
        QString scName = sc.name.toLower();
        QString scPath = QString(sc.path).replace('\\', '/').toLower();
        QString scExec = QString(sc.exec).replace('\\', '/').toLower();
        QString scFile = sc.file.toLower();

        QString bestMatch;
        int bestScore = 0;

        for (const auto &progVal : programs) {
            auto prog = progVal.toObject();
            QString k = prog.value("key").toString();
            auto m = meta.value(k.toLower());

            QString pName = (!prog.value("name").toString().isEmpty() ? prog.value("name").toString() : m.name).toLower();
            QString pLoc = m.loc.toLower();
            QString pGrp = m.group.toLower();
            QString pIcon = m.icon.toLower();

            int score = 0;
            if (!pLoc.isEmpty() && scPath.contains(pLoc)) {
                score += 50 + pLoc.length();
            }
            if (!pGrp.isEmpty() && scExec.contains(pGrp)) {
                score += 40 + pGrp.length();
            }
            if (!pName.isEmpty() && scExec.contains(pName)) {
                score += 30 + pName.length();
            }
            if (!pName.isEmpty() && scPath.contains(pName)) {
                score += 25 + pName.length();
            }
            if (!pName.isEmpty() && (pName == scName || pName.contains(scName) || scName.contains(pName))) {
                score += 20 + pName.length();
            }
            if (!pIcon.isEmpty() && scFile.contains(pIcon)) {
                score += 15;
            }

            if (score > bestScore) {
                bestScore = score;
                bestMatch = k;
            }
        }

        if (!bestMatch.isEmpty() && bestScore > 0) {
            QJsonObject scObj;
            scObj["id"] = sc.id;
            scObj["name"] = sc.name;
            scObj["icon"] = sc.icon;
            scObj["desktop"] = sc.desktop;
            scObj["menu"] = sc.menu;
            auto arr = grouped[bestMatch];
            arr.append(scObj);
            grouped[bestMatch] = arr;
        }
    }

    QJsonArray result;
    for (const auto &progVal : programs) {
        auto prog = progVal.toObject();
        QString k = prog.value("key").toString();
        auto arr = grouped.value(k);

        // Sort shortcuts by name case-insensitively
        QList<QJsonObject> list;
        for (const auto &v : arr) list.append(v.toObject());
        std::sort(list.begin(), list.end(), [](const QJsonObject &a, const QJsonObject &b) {
            return a.value("name").toString().toLower() < b.value("name").toString().toLower();
        });

        QJsonArray sortedArr;
        for (const auto &item : list) sortedArr.append(item);
        prog["shortcuts"] = sortedArr;
        result.append(prog);
    }
    return result;
}

QJsonObject toggleShortcut(
    const QString &prefix,
    const QString &shortcutId,
    const QVariant &desktop,
    const QVariant &menu,
    const QString &launcher,
    const QString &proton,
    const QString &customDataDir,
    const QString &customDesktopDir
) {
    QString pfx = QDir::cleanPath(prefix);
    QJsonObject config = readShortcutsConfig();
    QJsonObject pref = config.value(shortcutId).toObject();

    if (desktop.isValid()) {
        pref["desktop"] = desktop.toBool();
    }
    if (menu.isValid()) {
        pref["menu"] = menu.toBool();
    }
    config[shortcutId] = pref;
    saveShortcutsConfig(config);

    QString effLauncher = launcher;
    if (effLauncher.isEmpty()) {
        QString standardLauncher = QStandardPaths::findExecutable("winbridge");
        if (!standardLauncher.isEmpty()) {
            effLauncher = standardLauncher;
        } else {
            QString defaultLauncher = dataDir() + "/winbridge";
            effLauncher = QFile::exists(defaultLauncher) ? defaultLauncher : "/usr/local/bin/winbridge";
        }
    }

    QString effProton = proton;
    if (effProton.isEmpty()) {
        QJsonObject settings = readSettings();
        effProton = settings.value("proton").toString();
    }

    if (!effProton.isEmpty() && QFile::exists(effProton)) {
        importShortcuts(pfx, effProton, effLauncher, customDataDir, customDesktopDir.isEmpty() ? desktopDir() : customDesktopDir);
    }

    QJsonObject ret;
    ret["id"] = shortcutId;
    ret["desktop"] = pref.value("desktop").toBool(true);
    ret["menu"] = pref.value("menu").toBool(true);
    return ret;
}

QStringList importShortcuts(
    const QString &prefix,
    const QString &proton,
    const QString &launcher,
    const QString &customDataDir,
    const QString &customDesktopDir
) {
    QString pfx = QDir::cleanPath(prefix);
    QString data = customDataDir.isEmpty() ? (qEnvironmentVariable("XDG_DATA_HOME").isEmpty() ? QDir::homePath() + "/.local/share" : qEnvironmentVariable("XDG_DATA_HOME")) : customDataDir;
    QString desktop = customDesktopDir.isEmpty() ? desktopDir() : customDesktopDir;

    QString source = pfx + "/pfx/drive_c/proton_shortcuts";
    QDir srcDir(source);
    QJsonObject config = readShortcutsConfig();
    QStringList imported;

    for (const QString &file : srcDir.entryList({"*.desktop"}, QDir::Files, QDir::Name)) {
        QString scPath = srcDir.filePath(file);
        auto entry = parseDesktopFile(scPath);
        QString execVal = entry.value("Exec");
        QString target = shortcutTarget(execVal, pfx);
        if (target.isEmpty()) continue;

        QString name = entry.value("Name", QFileInfo(file).completeBaseName());
        if (isUninstallerShortcut(name, execVal)) continue;

        QByteArray idData = pfx.toUtf8() + '\0' + file.toUtf8();
        QString identity = QString::fromUtf8(QCryptographicHash::hash(idData, QCryptographicHash::Sha256).toHex()).left(20);
        QString filename = QString("winbridge-%1.desktop").arg(identity);

        QJsonObject pref = config.value(identity).toObject();
        bool showMenu = pref.contains("menu") ? pref.value("menu").toBool() : true;
        bool showDesktop = pref.contains("desktop") ? pref.value("desktop").toBool() : true;

        QString appDest = data + "/applications/" + filename;
        QString deskDest = desktop.isEmpty() ? QString() : desktop + "/" + filename;

        if (!showMenu && QFile::exists(appDest)) {
            QFile::remove(appDest);
        }
        if (!showDesktop && !deskDest.isEmpty() && QFile::exists(deskDest)) {
            QFile::remove(deskDest);
        }

        if (!showMenu && !showDesktop) {
            for (int s : {16, 22, 24, 32, 48, 64, 128, 256, 512}) {
                QString ipath = QString("%1/icons/hicolor/%2x%2/apps/winbridge-%3.png").arg(data).arg(s).arg(identity);
                QFile::remove(ipath);
            }
            continue;
        }

        QStringList command;
        if (launcher.endsWith(".py")) {
            command << "/usr/bin/python3" << launcher;
        } else {
            command << launcher;
        }

        if (pfx != sharedPrefix().trimmed()) {
            command << "--prefix" << pfx << "--proton" << proton;
        }
        command << "--" << target;

        QString icon = "application-x-executable";
        QString iconName = entry.value("Icon");
        if (iconName.endsWith(".png", Qt::CaseInsensitive)) {
            iconName.chop(4);
        }

        if (!iconName.isEmpty()) {
            QDir iconsDir(source + "/icons");
            if (iconsDir.exists()) {
                QMap<int, QString> resMap;
                int maxRes = 0;
                QString bestIconPath;

                for (const QString &subDir : iconsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                    int res = 0;
                    if (subDir.contains('x')) {
                        res = subDir.split('x')[0].toInt();
                    }
                    QString candidate = iconsDir.filePath(subDir + "/apps/" + iconName + ".png");
                    if (QFile::exists(candidate)) {
                        if (res > 0) resMap[res] = candidate;
                        if (res >= maxRes) {
                            maxRes = res;
                            bestIconPath = candidate;
                        }
                    }
                }

                if (!bestIconPath.isEmpty()) {
                    QString themeIconName = QString("winbridge-%1").arg(identity);
                    QImage bestImg;
                    bestImg.load(bestIconPath);

                    const QVector<int> standardSizes = {16, 22, 24, 32, 48, 64, 128, 256, 512};
                    for (int s : standardSizes) {
                        QString targetDir = QString("%1/icons/hicolor/%2x%2/apps").arg(data).arg(s);
                        QDir().mkpath(targetDir);
                        QString targetPath = QString("%1/%2.png").arg(targetDir, themeIconName);

                        if (resMap.contains(s)) {
                            QFile::remove(targetPath);
                            QFile::copy(resMap[s], targetPath);
                        } else if (!bestImg.isNull()) {
                            QImage scaled = bestImg.scaled(s, s, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                            scaled.save(targetPath, "PNG");
                        }
                    }
                    icon = themeIconName;
                }
            }
        }

        QStringList quotedCmd;
        for (const QString &arg : command) {
            quotedCmd << desktopQuote(arg);
        }

        QString content = QString("[Desktop Entry]\nType=Application\nName=%1\nExec=%2\nIcon=%3\nTerminal=false\nCategories=Game;\nComment=Start med WinBridge\n")
            .arg(field(name), quotedCmd.join(" "), field(icon));

        QStringList destinations;
        if (showMenu) destinations << appDest;
        if (showDesktop && !deskDest.isEmpty()) destinations << deskDest;

        for (const QString &destination : destinations) {
            QDir destDir = QFileInfo(destination).dir();
            if (!destDir.exists()) destDir.mkpath(".");

            bool needsWrite = true;
            QFile existing(destination);
            if (existing.open(QIODevice::ReadOnly)) {
                if (existing.readAll() == content.toUtf8()) {
                    needsWrite = false;
                }
                existing.close();
            }

            if (needsWrite) {
                QTemporaryFile temp(destination + ".tmp.XXXXXX");
                if (temp.open()) {
                    temp.write(content.toUtf8());
                    temp.flush();
                    QString tempPath = temp.fileName();
                    temp.close();
                    ::chmod(tempPath.toUtf8().constData(), 0755);
                    QFile::remove(destination);
                    QFile::rename(tempPath, destination);
                }
            }
        }
        imported.append(name);
    }

    QProcess updateProc;
    updateProc.start("update-desktop-database", {data + "/applications"});
    updateProc.waitForFinished(1000);

    if (customDataDir.isEmpty()) {
        QProcess iconProc;
        iconProc.start("gtk-update-icon-cache", {"-f", "-t", data + "/icons/hicolor"});
        iconProc.waitForFinished(2000);

        QProcess sycocaProc;
        sycocaProc.start("kbuildsycoca6", {"--noincremental"});
        sycocaProc.waitForFinished(3000);
    }

    return imported;
}

static void removeShortcutIcons(const QString &sourceDir, const QString &iconName) {
    if (iconName.isEmpty()) return;
    QString clean = iconName.endsWith(".png", Qt::CaseInsensitive) ? iconName.left(iconName.length() - 4) : iconName;
    QDir iconsDir(sourceDir + "/icons");
    if (!iconsDir.exists()) return;

    for (const QString &sub : iconsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QDir appsDir(iconsDir.filePath(sub + "/apps"));
        if (appsDir.exists()) {
            for (const QString &img : appsDir.entryList({clean + ".png", clean + "*.png"}, QDir::Files)) {
                QFile::remove(appsDir.filePath(img));
            }
            if (appsDir.entryList(QDir::Files).isEmpty()) {
                appsDir.rmdir(".");
            }
        }
        QDir subDir(iconsDir.filePath(sub));
        if (subDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
            iconsDir.rmdir(sub);
        }
    }

    QDir srcDir(sourceDir);
    for (const QString &loose : srcDir.entryList({clean + ".png", clean + ".ico", clean + ".xpm"}, QDir::Files)) {
        QFile::remove(srcDir.filePath(loose));
    }
}

void removeProgramShortcuts(
    const QString &prefix,
    const QString &programKey,
    const QString &programName,
    const ProgramRegistryMeta &meta,
    const QString &customDataDir,
    const QString &customDesktopDir
) {
    QString pfx = QDir::cleanPath(prefix);
    QString source = pfx + "/pfx/drive_c/proton_shortcuts";
    QDir srcDir(source);
    QString data = customDataDir.isEmpty() ? (qEnvironmentVariable("XDG_DATA_HOME").isEmpty() ? QDir::homePath() + "/.local/share" : qEnvironmentVariable("XDG_DATA_HOME")) : customDataDir;
    QString desktop = customDesktopDir.isEmpty() ? desktopDir() : customDesktopDir;

    QSet<QString> desktopFilesToRemove;
    QSet<QString> iconNamesToRemove;
    QSet<QString> identitiesToRemove;

    QJsonObject config = readShortcutsConfig();
    bool configChanged = false;

    QString pLoc = meta.loc.toLower().replace('\\', '/');
    while (pLoc.endsWith('/')) pLoc.chop(1);
    QString pGrp = meta.group.toLower();
    QString pName = programName.toLower();
    QString pKey = programKey.toLower();

    for (const QString &f : srcDir.entryList({"*.desktop"}, QDir::Files)) {
        QString fPath = srcDir.filePath(f);
        auto entry = parseDesktopFile(fPath);
        QString scName = entry.value("Name").toLower();
        QString scExec = entry.value("Exec").replace('\\', '/').toLower();
        QString scPath = entry.value("Path").replace('\\', '/').toLower();
        QString fLower = f.toLower();

        bool match = false;
        if (!pLoc.isEmpty() && (scPath.contains(pLoc) || scExec.contains(pLoc))) match = true;
        else if (!pGrp.isEmpty() && (scExec.contains(pGrp) || scPath.contains(pGrp) || fLower.contains(pGrp))) match = true;
        else if (!pName.isEmpty() && (scExec.contains(pName) || scPath.contains(pName) || scName == pName || scName.contains(pName) || pName.contains(scName))) match = true;
        else if (!pKey.isEmpty() && (scExec.contains(pKey) || scPath.contains(pKey) || fLower.contains(pKey))) match = true;

        if (match) {
            desktopFilesToRemove.insert(f);
            QString icon = entry.value("Icon");
            if (!icon.isEmpty()) {
                iconNamesToRemove.insert(icon);
            }
            QByteArray idData = pfx.toUtf8() + '\0' + f.toUtf8();
            QString identity = QString::fromUtf8(QCryptographicHash::hash(idData, QCryptographicHash::Sha256).toHex()).left(20);
            identitiesToRemove.insert(identity);
        }
    }

    for (const QString &f : desktopFilesToRemove) {
        QFile::remove(srcDir.filePath(f));
    }

    for (const QString &iconVal : iconNamesToRemove) {
        removeShortcutIcons(source, iconVal);
    }

    if (!meta.icon.isEmpty()) {
        QString baseIcon = QFileInfo(meta.icon).fileName();
        removeShortcutIcons(source, baseIcon);
    }

    for (const QString &id : identitiesToRemove) {
        QFile::remove(data + "/applications/winbridge-" + id + ".desktop");
        if (!desktop.isEmpty()) {
            QFile::remove(desktop + "/winbridge-" + id + ".desktop");
        }
        for (int s : {16, 22, 24, 32, 48, 64, 128, 256, 512}) {
            QFile::remove(QString("%1/icons/hicolor/%2x%2/apps/winbridge-%3.png").arg(data).arg(s).arg(id));
        }
        if (config.contains(id)) {
            config.remove(id);
            configChanged = true;
        }
    }

    if (configChanged) {
        saveShortcutsConfig(config);
    }

    QProcess updateProc;
    updateProc.start("update-desktop-database", {data + "/applications"});
    updateProc.waitForFinished(1000);
}

void cleanupOrphanedShortcuts(
    const QString &prefix,
    const QString &customDataDir,
    const QString &customDesktopDir
) {
    QString pfx = QDir::cleanPath(prefix);
    QString source = pfx + "/pfx/drive_c/proton_shortcuts";
    QDir srcDir(source);
    if (!srcDir.exists()) return;

    QString driveC = pfx + "/pfx/drive_c";
    QString data = customDataDir.isEmpty() ? (qEnvironmentVariable("XDG_DATA_HOME").isEmpty() ? QDir::homePath() + "/.local/share" : qEnvironmentVariable("XDG_DATA_HOME")) : customDataDir;
    QString desktop = customDesktopDir.isEmpty() ? desktopDir() : customDesktopDir;

    QJsonObject config = readShortcutsConfig();
    bool configChanged = false;

    QSet<QString> activeIconBases;

    std::function<bool(const QString&, int)> checkLnkTargetAlive;
    checkLnkTargetAlive = [&](const QString &lnkPath, int depth) -> bool {
        if (depth > 2) return false;
        QFile lnkFile(lnkPath);
        if (!lnkFile.open(QIODevice::ReadOnly)) return false;
        QByteArray lnkData = lnkFile.readAll();
        lnkFile.close();

        static QRegularExpression winPathRegex(R"([a-zA-Z]:\\[^\x00-\x1f"<>|?*]+)");
        QString asciiStr = QString::fromLatin1(lnkData);
        auto it = winPathRegex.globalMatch(asciiStr);
        bool foundSpecificTarget = false;
        bool specificTargetAlive = false;

        auto evaluatePath = [&](const QString &winPath) {
            QString lower = winPath.toLower();
            // Check for game/app target files
            if (lower.endsWith(".exe") || lower.endsWith(".isu") || lower.endsWith(".bat") || lower.endsWith(".cmd")) {
                // Ignore system executables (uninstaller, winhelp, cmd, etc.) when determining if the app itself is alive
                QString norm = QString(winPath).replace('\\', '/');
                QString base = QFileInfo(norm).fileName().toLower();
                if (base.contains("unins") || base.contains("unwise") || base.contains("isuninst") ||
                    base == "winhelp.exe" || base == "winhlp32.exe" || base == "hh.exe" ||
                    base == "cmd.exe" || base == "command.com" || base == "notepad.exe" || base == "regedit.exe") {
                    return;
                }
                foundSpecificTarget = true;
                QString rel = norm.mid(3);
                QString fullPath = driveC + "/" + rel;
                if (QFile::exists(fullPath)) {
                    specificTargetAlive = true;
                } else {
                    QFileInfo fi(fullPath);
                    QDir dir = fi.dir();
                    if (dir.exists()) {
                        QString bl = fi.fileName().toLower();
                        for (const QString &cand : dir.entryList(QDir::Files)) {
                            if (cand.toLower() == bl) {
                                specificTargetAlive = true;
                                break;
                            }
                        }
                    }
                }
            }
        };

        while (it.hasNext()) {
            evaluatePath(it.next().captured(0));
        }

        if (!foundSpecificTarget) {
            QString utf16Str = QString::fromUtf16(reinterpret_cast<const char16_t*>(lnkData.constData()), lnkData.size() / 2);
            auto it16 = winPathRegex.globalMatch(utf16Str);
            while (it16.hasNext()) {
                evaluatePath(it16.next().captured(0));
            }
        }

        if (depth > 0) {
            return foundSpecificTarget && specificTargetAlive;
        }

        if (!foundSpecificTarget) {
            QDir dir = QFileInfo(lnkPath).dir();
            bool hasLivingSibling = false;
            for (const QString &sibling : dir.entryList({"*.lnk"}, QDir::Files)) {
                if (dir.filePath(sibling) == lnkPath) continue;
                if (checkLnkTargetAlive(dir.filePath(sibling), depth + 1)) {
                    hasLivingSibling = true;
                    break;
                }
            }
            if (!hasLivingSibling) {
                return false;
            }
        }

        if (foundSpecificTarget && !specificTargetAlive) {
            return false;
        }
        return true;
    };

    for (const QString &file : srcDir.entryList({"*.desktop"}, QDir::Files, QDir::Name)) {
        QString scPath = srcDir.filePath(file);
        auto entry = parseDesktopFile(scPath);
        QString execVal = entry.value("Exec");
        QString target = shortcutTarget(execVal, pfx);
        QString iconName = entry.value("Icon");

        bool dead = false;
        if (target.isEmpty() || !QFile::exists(target)) {
            dead = true;
        } else if (target.endsWith(".lnk", Qt::CaseInsensitive)) {
            if (!checkLnkTargetAlive(target, 0)) {
                dead = true;
            }
        }

        QByteArray idData = pfx.toUtf8() + '\0' + file.toUtf8();
        QString identity = QString::fromUtf8(QCryptographicHash::hash(idData, QCryptographicHash::Sha256).toHex()).left(20);

        if (dead) {
            QFile::remove(scPath);
            removeShortcutIcons(source, iconName);

            QFile::remove(data + "/applications/winbridge-" + identity + ".desktop");
            if (!desktop.isEmpty()) {
                QFile::remove(desktop + "/winbridge-" + identity + ".desktop");
            }
            for (int s : {16, 22, 24, 32, 48, 64, 128, 256, 512}) {
                QFile::remove(QString("%1/icons/hicolor/%2x%2/apps/winbridge-%3.png").arg(data).arg(s).arg(identity));
            }
            if (config.contains(identity)) {
                config.remove(identity);
                configChanged = true;
            }
        } else {
            if (!iconName.isEmpty()) {
                QString clean = iconName.endsWith(".png", Qt::CaseInsensitive) ? iconName.left(iconName.length() - 4) : iconName;
                activeIconBases.insert(clean.toLower());
                activeIconBases.insert(iconName.toLower());
            }
        }
    }

    QDir iconsDir(source + "/icons");
    if (iconsDir.exists()) {
        for (const QString &sub : iconsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QDir appsDir(iconsDir.filePath(sub + "/apps"));
            if (appsDir.exists()) {
                for (const QString &img : appsDir.entryList({"*.png"}, QDir::Files)) {
                    QString clean = img.endsWith(".png", Qt::CaseInsensitive) ? img.left(img.length() - 4) : img;
                    if (!activeIconBases.contains(clean.toLower()) && !activeIconBases.contains(img.toLower())) {
                        QFile::remove(appsDir.filePath(img));
                    }
                }
                if (appsDir.entryList(QDir::Files).isEmpty()) {
                    appsDir.rmdir(".");
                }
            }
            QDir subDir(iconsDir.filePath(sub));
            if (subDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
                iconsDir.rmdir(sub);
            }
        }
    }

    for (const QString &loose : srcDir.entryList({"*.png", "*.ico", "*.xpm"}, QDir::Files)) {
        QString clean = loose;
        int dot = clean.lastIndexOf('.');
        if (dot > 0) clean = clean.left(dot);
        if (!activeIconBases.contains(clean.toLower()) && !activeIconBases.contains(loose.toLower())) {
            QFile::remove(srcDir.filePath(loose));
        }
    }

    QDir appDir(data + "/applications");
    for (const QString &f : appDir.entryList({"winbridge-*.desktop"}, QDir::Files)) {
        if (f == "winbridge-manager.desktop") continue;
        QString p = appDir.filePath(f);
        auto entry = parseDesktopFile(p);
        QString execVal = entry.value("Exec");

        QString target;
        static QRegularExpression lastArgRegex(R"("[^"]+"|\S+)");
        auto matchIt = lastArgRegex.globalMatch(execVal);
        QString lastToken;
        while (matchIt.hasNext()) {
            lastToken = matchIt.next().captured(0);
        }
        if (lastToken.startsWith('"') && lastToken.endsWith('"') && lastToken.length() >= 2) {
            target = lastToken.mid(1, lastToken.length() - 2);
        } else {
            target = lastToken;
        }

        if (!target.isEmpty()) {
            bool dead = !QFile::exists(target);
            if (!dead && target.endsWith(".lnk", Qt::CaseInsensitive)) {
                dead = !checkLnkTargetAlive(target, 0);
            }
            if (!dead && isUninstallerShortcut(entry.value("Name"), execVal)) {
                dead = true;
            }
            if (dead) {
                QFile::remove(p);
                QString id = f.mid(10, f.length() - 18);
                if (!desktop.isEmpty()) {
                    QFile::remove(desktop + "/" + f);
                }
                for (int s : {16, 22, 24, 32, 48, 64, 128, 256, 512}) {
                    QFile::remove(QString("%1/icons/hicolor/%2x%2/apps/winbridge-%3.png").arg(data).arg(s).arg(id));
                }
                if (config.contains(id)) {
                    config.remove(id);
                    configChanged = true;
                }
            }
        }
    }

    if (configChanged) {
        saveShortcutsConfig(config);
    }
}

} // namespace WinBridge
