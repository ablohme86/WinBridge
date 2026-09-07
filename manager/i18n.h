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
