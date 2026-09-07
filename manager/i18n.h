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

#pragma once
#include <QtCore>
namespace I18n {
inline QJsonObject strings;
inline void load(const QString &locale) {
    strings = {};
    // English source strings are the fallback for missing or invalid translations.
    if (locale == "en-US") return;
    QFile file(":/i18n/" + locale + ".i18n");
    if (file.open(QIODevice::ReadOnly)) strings = QJsonDocument::fromJson(file.readAll()).object()["strings"].toObject();
}
}
inline QString T(const QString &source) {
    QString translated = I18n::strings.value(source).toString();
    return translated.isEmpty() ? source : translated;
}
