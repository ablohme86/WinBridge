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

#include "manager_backend.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

using namespace WinBridge;

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();

    QCommandLineOption keyOpt("key", "Program key", "key");
    QCommandLineOption protonOpt("proton", "Proton path", "proton");
    QCommandLineOption prefixOpt("prefix", "Prefix path", "prefix");
    QCommandLineOption idOpt("id", "Shortcut ID", "id");
    QCommandLineOption desktopOpt("desktop", "Desktop toggle (0 or 1)", "desktop");
    QCommandLineOption menuOpt("menu", "Menu toggle (0 or 1)", "menu");

    parser.addOption(keyOpt);
    parser.addOption(protonOpt);
    parser.addOption(prefixOpt);
    parser.addOption(idOpt);
    parser.addOption(desktopOpt);
    parser.addOption(menuOpt);

    parser.addPositionalArgument("action", "Action: list, uninstall, settings, configure, toggle_shortcut, running, kill");

    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        std::cerr << "Action is required.\n";
        return 1;
    }

    QString action = positional.first();
    QString key = parser.value(keyOpt);
    QString proton = parser.value(protonOpt);
    QString prefix = parser.value(prefixOpt);
    QString shortcutId = parser.value(idOpt);

    QVariant desktopVar;
    if (parser.isSet(desktopOpt)) {
        desktopVar = parser.value(desktopOpt).toInt() != 0;
    }

    QVariant menuVar;
    if (parser.isSet(menuOpt)) {
        menuVar = parser.value(menuOpt).toInt() != 0;
    }

    try {
        QJsonObject result = operate(action, key, proton, prefix, shortcutId, desktopVar, menuVar);
        std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << "\n";
        return 0;
    } catch (const std::exception &exc) {
        QJsonObject err;
        err["error"] = QString::fromUtf8(exc.what());
        std::cout << QJsonDocument(err).toJson(QJsonDocument::Compact).toStdString() << "\n";
        return 1;
    }
}
