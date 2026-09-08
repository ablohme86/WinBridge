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
#include <QtWidgets>
#include <QProcess>
#include "i18n.h"

inline QString retroStyleSheet(QString theme = QString()) {
    if (theme.isEmpty()) {
        theme = QSettings("WinBridge", "Manager").value("theme", "classic").toString();
    }
    bool dark = (theme == "dark");

    QString btnFace = dark ? "#2c2f35" : "#c0c0c0";
    QString btnLight = dark ? "#525763" : "#ffffff";
    QString btnShadow = dark ? "#181a1d" : "#808080";
    QString btnDarkShadow = dark ? "#0e0f11" : "#404040";
    QString windowBg = dark ? "#17181c" : "#ffffff";
    QString textMain = dark ? "#eef0f3" : "#000000";
    QString textMuted = dark ? "#9aa0aa" : "#444444";
    QString accent = dark ? "#7ea8f8" : "#000080";
    QString accentSecondary = dark ? "#9fc0fc" : "#004080";
    QString topNavGrad = dark
        ? "qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #101624, stop:0.75 #182236, stop:1 #243450)"
        : "qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #000080, stop:0.75 #083b82, stop:1 #1084d0)";
    QString cardBorder = dark ? "2px groove #525763" : "2px groove #ffffff";
    QString sunkenBg = dark ? "#222429" : "#c0c0c0";
    QString itemHover = dark ? "#222733" : "#f0f4fc";
    QString itemSelect = dark ? "#193563" : "#c8daf8";
    QString itemSelectBorder = dark ? "#5892f3" : "#000080";
    QString shortcutsBg = dark ? "#222429" : "#ece9d8";
    QString scrollTrack = dark ? "#1e2024" : "#d4d0c8";
    QString btnHover = dark ? "#373b43" : "#d4d4d4";
    QString btnPressed = dark ? "#202226" : "#b8b8b8";
    QString dangerHoverBg = dark ? "#4c2424" : "#dec8c8";
    QString dangerHoverText = dark ? "#ff8888" : "#800000";
    QString listDivider = dark ? "#26282e" : "#d4d0c8";
    QString runningBg = dark ? "#107c10" : "#008000";
    QString runningTop = dark ? "#3cb13c" : "#50d050";
    QString runningBottom = dark ? "#0a4d0a" : "#004000";
    QString focusBorder = dark ? "#ffffff" : "#000000";

    QString s = QString::fromUtf8(R"(
        QWidget {
            background: {{btnFace}};
            color: {{textMain}};
            font-family: 'Segoe UI', 'Tahoma', 'MS Sans Serif', 'DejaVu Sans', sans-serif;
            font-size: 12px;
        }
        QFrame#topNav {
            background: {{topNavGrad}};
            border-bottom: 2px solid {{btnShadow}};
        }
        QLabel {
            background: transparent;
        }
        QLabel#brand {
            font-size: 30px;
            font-weight: bold;
            color: #ffffff;
            letter-spacing: -0.5px;
        }
        QLabel#brandSub {
            font-size: 13px;
            font-weight: bold;
            color: #ffeb80;
            letter-spacing: 1.5px;
        }
        QLabel#eyebrow {
            color: {{accent}};
            font-size: 11px;
            font-weight: bold;
            letter-spacing: 0.5px;
        }
        QLabel#heading {
            font-size: 22px;
            font-weight: bold;
            color: {{accent}};
        }
        QLabel#muted {
            color: {{textMuted}};
            font-size: 12px;
        }
        QLabel#count {
            font-size: 26px;
            font-weight: bold;
            color: {{accent}};
        }
        QLabel#engine {
            font-size: 13px;
            font-weight: bold;
            color: {{accentSecondary}};
        }
        QLabel#envPath {
            font-size: 12px;
            font-weight: bold;
            color: {{textMain}};
        }
        QFrame#heroBanner {
            background: {{btnFace}};
            border: {{cardBorder}};
            border-radius: 0px;
        }
        QFrame#statCard {
            background: {{sunkenBg}};
            border-top: 2px solid {{btnShadow}};
            border-left: 2px solid {{btnShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            border-radius: 0px;
            padding: 5px 10px;
        }
        QFrame#detail {
            background: {{btnFace}};
            border: {{cardBorder}};
            border-radius: 0px;
        }
        QGroupBox {
            background: {{btnFace}};
            border: {{cardBorder}};
            border-radius: 0px;
            margin-top: 10px;
            padding-top: 10px;
            font-weight: bold;
            color: {{textMain}};
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 8px;
            padding: 0 4px;
            background: {{btnFace}};
        }
        QLabel#appIcon {
            background: {{btnFace}};
            color: {{accent}};
            border-top: 1px solid {{btnLight}};
            border-left: 1px solid {{btnLight}};
            border-right: 1px solid {{btnShadow}};
            border-bottom: 1px solid {{btnShadow}};
            border-radius: 0px;
            font-weight: bold;
            font-size: 18px;
        }
        QLabel#badge {
            background: {{sunkenBg}};
            color: {{accent}};
            border-top: 2px solid {{btnShadow}};
            border-left: 2px solid {{btnShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            border-radius: 0px;
            font-size: 38px;
            font-weight: bold;
        }
        QLabel#appName {
            font-size: 14px;
            font-weight: bold;
            color: {{textMain}};
        }
        QLabel#installed {
            background: {{btnFace}};
            color: {{accent}};
            border-top: 1px solid {{btnShadow}};
            border-left: 1px solid {{btnShadow}};
            border-right: 1px solid {{btnLight}};
            border-bottom: 1px solid {{btnLight}};
            border-radius: 0px;
            padding: 2px 8px;
            font-size: 11px;
            font-weight: bold;
        }
        QLabel#running {
            background: {{runningBg}};
            color: #ffffff;
            border-top: 1px solid {{runningTop}};
            border-left: 1px solid {{runningTop}};
            border-right: 1px solid {{runningBottom}};
            border-bottom: 1px solid {{runningBottom}};
            border-radius: 0px;
            padding: 2px 8px;
            font-size: 11px;
            font-weight: bold;
        }
        QLabel#detailName {
            font-size: 18px;
            font-weight: bold;
            color: {{accent}};
        }
        QPushButton {
            background: {{btnFace}};
            color: {{textMain}};
            border-top: 2px solid {{btnLight}};
            border-left: 2px solid {{btnLight}};
            border-right: 2px solid {{btnDarkShadow}};
            border-bottom: 2px solid {{btnDarkShadow}};
            border-radius: 0px;
            padding: 5px 13px;
            font-weight: bold;
            font-size: 12px;
        }
        QPushButton:hover {
            background: {{btnHover}};
        }
        QPushButton:pressed {
            background: {{btnPressed}};
            border-top: 2px solid {{btnDarkShadow}};
            border-left: 2px solid {{btnDarkShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            padding-top: 6px;
            padding-left: 14px;
            padding-right: 12px;
            padding-bottom: 4px;
        }
        QPushButton:focus {
            outline: 1px dotted {{textMain}};
        }
        QPushButton:disabled {
            color: {{btnShadow}};
            background: {{btnFace}};
            border-top: 2px solid {{btnLight}};
            border-left: 2px solid {{btnLight}};
            border-right: 2px solid {{btnShadow}};
            border-bottom: 2px solid {{btnShadow}};
        }
        QPushButton#primary {
            background: {{btnFace}};
            color: {{textMain}};
            border-top: 2px solid {{btnLight}};
            border-left: 2px solid {{btnLight}};
            border-right: 2px solid {{btnDarkShadow}};
            border-bottom: 2px solid {{btnDarkShadow}};
            border-radius: 0px;
            font-weight: bold;
        }
        QPushButton#primary:hover {
            background: {{btnHover}};
        }
        QPushButton#primary:pressed {
            border-top: 2px solid {{btnDarkShadow}};
            border-left: 2px solid {{btnDarkShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            background: {{btnPressed}};
        }
        QPushButton#primary:disabled {
            color: {{btnShadow}};
            background: {{btnFace}};
            border-color: {{btnShadow}};
        }
        QPushButton#danger {
            background: {{btnFace}};
            color: {{textMain}};
            border-top: 2px solid {{btnLight}};
            border-left: 2px solid {{btnLight}};
            border-right: 2px solid {{btnDarkShadow}};
            border-bottom: 2px solid {{btnDarkShadow}};
            border-radius: 0px;
            font-weight: bold;
        }
        QPushButton#danger:hover {
            background: {{dangerHoverBg}};
            color: {{dangerHoverText}};
        }
        QPushButton#danger:pressed {
            border-top: 2px solid {{btnDarkShadow}};
            border-left: 2px solid {{btnDarkShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            background: {{btnPressed}};
        }
        QPushButton#danger:disabled {
            background: {{btnFace}};
            color: {{btnShadow}};
            border-color: {{btnShadow}};
        }
        QPushButton#itemKill, QPushButton#killAppButton {
            background: {{btnFace}};
            color: {{dangerHoverText}};
            border-top: 2px solid {{btnLight}};
            border-left: 2px solid {{btnLight}};
            border-right: 2px solid {{btnDarkShadow}};
            border-bottom: 2px solid {{btnDarkShadow}};
            border-radius: 0px;
            padding: 3px 10px;
            font-size: 11px;
            font-weight: bold;
        }
        QPushButton#itemKill:hover, QPushButton#killAppButton:hover {
            background: {{dangerHoverBg}};
            color: {{dangerHoverText}};
        }
        QPushButton#itemKill:pressed, QPushButton#killAppButton:pressed {
            border-top: 2px solid {{btnDarkShadow}};
            border-left: 2px solid {{btnDarkShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            background: {{btnPressed}};
        }
        QPushButton#killAppButton:disabled {
            background: {{btnFace}};
            border-color: {{btnShadow}};
            color: {{btnShadow}};
        }
        QPushButton#itemUninstall {
            background: {{btnFace}};
            color: {{textMain}};
            border-top: 2px solid {{btnLight}};
            border-left: 2px solid {{btnLight}};
            border-right: 2px solid {{btnDarkShadow}};
            border-bottom: 2px solid {{btnDarkShadow}};
            border-radius: 0px;
            padding: 3px 10px;
            font-size: 11px;
            font-weight: bold;
        }
        QPushButton#itemUninstall:hover {
            background: {{dangerHoverBg}};
            color: {{dangerHoverText}};
        }
        QPushButton#itemUninstall:pressed {
            border-top: 2px solid {{btnDarkShadow}};
            border-left: 2px solid {{btnDarkShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            background: {{btnPressed}};
        }
        QPushButton#itemUninstall:disabled {
            background: {{btnFace}};
            border-color: {{btnShadow}};
            color: {{btnShadow}};
        }
        QPushButton#filterTab {
            background: {{btnFace}};
            color: {{textMain}};
            border-top: 2px solid {{btnLight}};
            border-left: 2px solid {{btnLight}};
            border-right: 2px solid {{btnDarkShadow}};
            border-bottom: 2px solid {{btnDarkShadow}};
            border-radius: 0px;
            padding: 5px 14px;
            font-weight: bold;
            font-size: 12px;
        }
        QPushButton#filterTab:hover {
            background: {{btnHover}};
        }
        QPushButton#filterTab:checked {
            background: {{btnHover}};
            border-top: 2px solid {{btnDarkShadow}};
            border-left: 2px solid {{btnDarkShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            color: {{accent}};
        }
        QPushButton#shortcutToggle {
            background: {{btnFace}};
            border-top: 2px solid {{btnLight}};
            border-left: 2px solid {{btnLight}};
            border-right: 2px solid {{btnDarkShadow}};
            border-bottom: 2px solid {{btnDarkShadow}};
            border-radius: 0px;
            padding: 3px 10px;
            font-size: 11px;
            font-weight: bold;
            color: {{textMain}};
        }
        QPushButton#shortcutToggle:hover {
            background: {{btnHover}};
        }
        QPushButton#shortcutToggle:pressed {
            border-top: 2px solid {{btnDarkShadow}};
            border-left: 2px solid {{btnDarkShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
        }
        QPushButton#shortcutToggle:disabled {
            color: {{btnShadow}};
            background: {{btnFace}};
            border-color: {{btnShadow}};
        }
        QFrame#shortcutsPanel {
            background: {{shortcutsBg}};
            border-top: 1px solid {{btnShadow}};
            border-left: 1px solid {{btnShadow}};
            border-right: 1px solid {{btnLight}};
            border-bottom: 1px solid {{btnLight}};
            border-radius: 0px;
            padding: 8px 16px 12px 16px;
        }
        QLabel#miniAppIcon {
            background: {{btnFace}};
            color: {{accent}};
            border-top: 1px solid {{btnLight}};
            border-left: 1px solid {{btnLight}};
            border-right: 1px solid {{btnShadow}};
            border-bottom: 1px solid {{btnShadow}};
            border-radius: 0px;
            font-weight: bold;
            font-size: 11px;
        }
        QLabel#shortcutName {
            font-size: 12px;
            font-weight: bold;
            color: {{textMain}};
        }
        QCheckBox {
            color: {{textMain}};
            spacing: 6px;
            font-size: 11px;
            font-weight: bold;
        }
        QCheckBox::indicator {
            width: 13px;
            height: 13px;
            border-top: 2px solid {{btnShadow}};
            border-left: 2px solid {{btnShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            background: {{windowBg}};
            border-radius: 0px;
        }
        QCheckBox::indicator:checked {
            background: {{accent}};
            border-top: 2px solid {{btnDarkShadow}};
            border-left: 2px solid {{btnDarkShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
        }
        QLineEdit#search, QLineEdit#prefix {
            background: {{windowBg}};
            border-top: 2px solid {{btnShadow}};
            border-left: 2px solid {{btnShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            border-radius: 0px;
            padding: 5px 8px;
            selection-background-color: {{accent}};
            selection-color: #ffffff;
            color: {{textMain}};
            font-size: 12px;
        }
        QLineEdit#search:focus, QLineEdit#prefix:focus {
            border-top: 2px solid {{focusBorder}};
            border-left: 2px solid {{focusBorder}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
        }
        QWidget#itemContainer, QWidget#itemCard {
            background: transparent;
        }
        QListWidget#programList {
            background: {{windowBg}};
            border-top: 2px solid {{btnShadow}};
            border-left: 2px solid {{btnShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            border-radius: 0px;
            outline: 0;
            padding: 0px;
        }
        QListWidget#programList::item {
            background: {{windowBg}};
            border-bottom: 1px solid {{listDivider}};
            border-radius: 0px;
            margin-bottom: 0px;
        }
        QListWidget#programList::item:hover {
            background: {{itemHover}};
        }
        QListWidget#programList::item:selected {
            background: {{itemSelect}};
            border: 2px solid {{itemSelectBorder}};
        }
        QScrollBar:vertical {
            background: {{scrollTrack}};
            width: 16px;
            margin: 0px;
            border: 1px solid {{btnShadow}};
        }
        QScrollBar::handle:vertical {
            background: {{btnFace}};
            min-height: 20px;
            border-top: 2px solid {{btnLight}};
            border-left: 2px solid {{btnLight}};
            border-right: 2px solid {{btnDarkShadow}};
            border-bottom: 2px solid {{btnDarkShadow}};
        }
        QScrollBar::handle:vertical:pressed {
            background: {{btnPressed}};
            border-top: 2px solid {{btnDarkShadow}};
            border-left: 2px solid {{btnDarkShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QProgressBar {
            border-top: 1px solid {{btnShadow}};
            border-left: 1px solid {{btnShadow}};
            border-right: 1px solid {{btnLight}};
            border-bottom: 1px solid {{btnLight}};
            background: {{windowBg}};
            max-height: 12px;
        }
        QProgressBar::chunk {
            background: {{accent}};
        }
        QLabel#statusBar {
            background: {{btnFace}};
            border-top: 2px solid {{btnShadow}};
            border-left: 2px solid {{btnShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            padding: 3px 8px;
            font-size: 11px;
            color: {{textMain}};
        }
        QComboBox {
            background: {{windowBg}};
            color: {{textMain}};
            border-top: 2px solid {{btnShadow}};
            border-left: 2px solid {{btnShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            border-radius: 0px;
            padding: 4px 6px;
            font-size: 12px;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 18px;
            background: {{btnFace}};
            border-left: 1px solid {{btnShadow}};
            border-top: 1px solid {{btnLight}};
            border-bottom: 1px solid {{btnDarkShadow}};
            border-right: 1px solid {{btnDarkShadow}};
        }
        QComboBox QAbstractItemView {
            background: {{windowBg}};
            color: {{textMain}};
            selection-background-color: {{accent}};
            selection-color: #ffffff;
            border: 1px solid {{btnShadow}};
        }
        QDialog {
            background: {{btnFace}};
            color: {{textMain}};
        }
        QLabel#aboutTitle {
            font-size: 17px;
            font-weight: bold;
            color: {{accent}};
        }
        QLabel#aboutVersion {
            font-size: 12px;
            font-weight: bold;
            color: {{textMain}};
        }
        QLabel#aboutTagline, QLabel#aboutCopyright, QLabel#aboutLicense {
            font-size: 11px;
            color: {{textMuted}};
        }
        QFrame#aboutDivider {
            border-top: 1px solid {{btnShadow}};
            border-bottom: 1px solid {{btnLight}};
            height: 2px;
            max-height: 2px;
        }
        QFrame#aboutInfoBox {
            background: {{sunkenBg}};
            border-top: 2px solid {{btnShadow}};
            border-left: 2px solid {{btnShadow}};
            border-right: 2px solid {{btnLight}};
            border-bottom: 2px solid {{btnLight}};
            padding: 8px 12px;
        }
        QLabel#aboutHeading {
            font-size: 11px;
            font-weight: bold;
            color: {{accent}};
        }
        QLabel#aboutUserInfo {
            font-size: 11px;
            font-weight: bold;
            color: {{textMain}};
        }
        QLabel#aboutGplNotice {
            font-size: 10px;
            color: {{textMain}};
        }
        QLabel#aboutSysInfo {
            font-size: 11px;
            font-family: 'Consolas', 'Courier New', monospace;
            color: {{textMain}};
        }
    )");

    s.replace("{{btnFace}}", btnFace);
    s.replace("{{btnLight}}", btnLight);
    s.replace("{{btnShadow}}", btnShadow);
    s.replace("{{btnDarkShadow}}", btnDarkShadow);
    s.replace("{{windowBg}}", windowBg);
    s.replace("{{textMain}}", textMain);
    s.replace("{{textMuted}}", textMuted);
    s.replace("{{accent}}", accent);
    s.replace("{{accentSecondary}}", accentSecondary);
    s.replace("{{topNavGrad}}", topNavGrad);
    s.replace("{{cardBorder}}", cardBorder);
    s.replace("{{sunkenBg}}", sunkenBg);
    s.replace("{{itemHover}}", itemHover);
    s.replace("{{itemSelect}}", itemSelect);
    s.replace("{{itemSelectBorder}}", itemSelectBorder);
    s.replace("{{shortcutsBg}}", shortcutsBg);
    s.replace("{{scrollTrack}}", scrollTrack);
    s.replace("{{btnHover}}", btnHover);
    s.replace("{{btnPressed}}", btnPressed);
    s.replace("{{dangerHoverBg}}", dangerHoverBg);
    s.replace("{{dangerHoverText}}", dangerHoverText);
    s.replace("{{listDivider}}", listDivider);
    s.replace("{{runningBg}}", runningBg);
    s.replace("{{runningTop}}", runningTop);
    s.replace("{{runningBottom}}", runningBottom);
    s.replace("{{focusBorder}}", focusBorder);

    return s;
}

class SettingsDialog : public QDialog {
    QComboBox *language, *theme, *proton;
    QLineEdit *prefix;
    QPushButton *browse;
    QLabel *message;
    QPushButton *save, *cancel;
    QComboBox *downloadVersion;
    QPushButton *download;
    QProgressBar *downloadProgress;
    QProcess *process;
    QString backend, originalLanguage, originalTheme, originalProton, originalPrefix, defaultPrefix;
    bool saving = false, installing = false, inFlight = false, loaded = false;
    QByteArray output;
    void run(const QStringList &args) {
        inFlight=true; output.clear();save->setEnabled(false);cancel->setEnabled(!saving && !installing);
        language->setEnabled(false);theme->setEnabled(false);proton->setEnabled(false);
        prefix->setEnabled(false);browse->setEnabled(false);
        download->setEnabled(false);downloadVersion->setEnabled(false);
        if (backend.endsWith(".py")) {
            process->start("/usr/bin/python3", QStringList{backend} + args);
        } else {
            process->start(backend, args);
        }
    }
    void showStatus(const QString &text) {
        inFlight=false;saving=false;installing=false;downloadProgress->hide();message->setText(T(text));cancel->setEnabled(true);
        language->setEnabled(true);theme->setEnabled(true);proton->setEnabled(true);
        prefix->setEnabled(true);browse->setEnabled(true);save->setEnabled(loaded);
        download->setEnabled(loaded);downloadVersion->setEnabled(loaded);
    }
    void populateProtons(const QJsonObject &result, const QString &preferred) {
        proton->clear();
        proton->addItem(T(originalProton.isEmpty() ? "Not selected" : "Keep current version"),QString());
        for(const auto &v:result["versions"].toArray()) {
            auto p=v.toObject();proton->addItem(T(p["name"].toString()),p["path"].toString());
        }
        int selected=proton->findData(preferred);
        proton->setCurrentIndex(selected<0?0:selected);
    }
    void finish(int code, QProcess::ExitStatus state) {
        output+=process->readAllStandardOutput();
        QJsonParseError parse;
        auto doc=QJsonDocument::fromJson(output,&parse);
        auto result=doc.object();
        if(code!=0 || state!=QProcess::NormalExit || parse.error!=QJsonParseError::NoError || !doc.isObject() || result.contains("error")) {
            showStatus(result.value("error").toString(installing ? "Could not download Proton." : (saving ? "Could not save settings." : "Could not load settings.")));return;
        }
        inFlight=false;
        if(installing) {
            populateProtons(result,downloadVersion->currentData().toString());
            showStatus("Proton is ready. Choose a version and save your changes.");
            return;
        }
        if(saving) {
            QSettings settings("WinBridge","Manager");
            settings.setValue("language",language->currentData().toString());
            settings.setValue("theme",theme->currentData().toString());
            settings.sync();
            if(settings.status()!=QSettings::NoError){showStatus("Could not save settings.");return;}
            accept();return;
        }
        originalProton=result["selected"].toString();
        loaded=true;
        originalPrefix=result.value("prefix").toString();
        defaultPrefix=result.value("default_prefix").toString();
        if(originalPrefix.isEmpty() && !defaultPrefix.isEmpty()) originalPrefix = defaultPrefix;
        prefix->setText(originalPrefix);
        if(!defaultPrefix.isEmpty()) prefix->setPlaceholderText(defaultPrefix);

        populateProtons(result,originalProton);
        int selected=proton->findData(originalProton);
        if(result.contains("umu_available") && !result["umu_available"].toBool())
            showStatus("Install umu-launcher to download and run Proton without Steam. See INSTALL.md for installation instructions.");
        else if(selected<0&&!originalProton.isEmpty()) showStatus("Saved version is no longer installed. Please choose another version.");
        else showStatus("Close Windows apps before changing Proton versions.");
    }
public:
    bool languageChanged() const {return language->currentData().toString()!=originalLanguage;}
    bool themeChanged() const {return theme->currentData().toString()!=originalTheme;}
    QString selectedTheme() const {return theme->currentData().toString();}
    void reject() override {
        if((saving || installing) && inFlight) return;
        if(process->state()!=QProcess::NotRunning){process->kill();process->waitForFinished(1000);}
        QDialog::reject();
    }
    SettingsDialog(const QString &path,QWidget *parent=nullptr):QDialog(parent),backend(path) {
        setWindowTitle(T("Settings"));setMinimumWidth(560);
        originalLanguage=QSettings("WinBridge","Manager").value("language","en-US").toString();
        originalTheme=QSettings("WinBridge","Manager").value("theme","classic").toString();
        setStyleSheet(retroStyleSheet(originalTheme));
        auto *layout=new QVBoxLayout(this);layout->setContentsMargins(30,22,30,22);layout->setSpacing(14);
        auto label=[&](const QString &text,const char *id=nullptr){auto *l=new QLabel(T(text));l->setTextFormat(Qt::PlainText);l->setWordWrap(true);if(id)l->setObjectName(id);return l;};
        layout->addWidget(label("Settings","heading"));layout->addWidget(label("Personalize your workspace","muted"));layout->addSpacing(8);
        
        layout->addWidget(label("Language"));language=new QComboBox;language->setObjectName("language");
        language->addItem("English","en-US");language->addItem("Norsk bokmål","no-NB");
        language->setCurrentIndex(qMax(0,language->findData(originalLanguage)));layout->addWidget(language);
        layout->addWidget(label("Choose the language used by WinBridge Manager.","muted"));layout->addSpacing(8);

        layout->addWidget(label("Theme"));theme=new QComboBox;theme->setObjectName("theme");
        theme->addItem(T("Classic Light (Windows 98)"),"classic");
        theme->addItem(T("Retro Dark (Plus! Mystery)"),"dark");
        theme->setCurrentIndex(qMax(0,theme->findData(originalTheme)));layout->addWidget(theme);
        layout->addWidget(label("Choose your color scheme.","muted"));layout->addSpacing(8);

        connect(theme, &QComboBox::currentIndexChanged, this, [this] {
            setStyleSheet(retroStyleSheet(theme->currentData().toString()));
        });

        layout->addWidget(label("Proton version"));proton=new QComboBox;proton->setObjectName("proton");layout->addWidget(proton);
        layout->addWidget(label("Used by all apps in your shared Windows environment.","muted"));layout->addSpacing(8);
        auto *downloadRow=new QHBoxLayout;
        downloadVersion=new QComboBox;downloadVersion->setObjectName("downloadVersion");
        downloadVersion->addItem("GE-Proton","GE-Proton");
        downloadVersion->addItem("UMU-Proton","UMU-Proton");
        downloadRow->addWidget(downloadVersion,1);
        download=new QPushButton(QIcon(":/icons/download.png"), T("Download Proton"));download->setIconSize(QSize(16, 16));download->setObjectName("downloadProton");downloadRow->addWidget(download);
        layout->addLayout(downloadRow);
        layout->addWidget(label("Downloads Proton and the required runtime. Steam is not required. Automatic versions check for updates when launched.","muted"));
        downloadProgress=new QProgressBar;downloadProgress->setObjectName("downloadProgress");downloadProgress->setRange(0,0);downloadProgress->hide();layout->addWidget(downloadProgress);

        layout->addWidget(label("Proton / Wine install directory"));
        auto *prefixRow=new QHBoxLayout;prefixRow->setSpacing(8);
        prefix=new QLineEdit;prefix->setObjectName("prefix");prefixRow->addWidget(prefix,1);
        browse=new QPushButton(QIcon(":/icons/show-files.png"), T("Browse …"));browse->setIconSize(QSize(16, 16));browse->setObjectName("browse");prefixRow->addWidget(browse);
        layout->addLayout(prefixRow);
        layout->addWidget(label("Location of your shared Windows environment and installed apps.","muted"));

        message=label("Loading installed Proton versions …","muted");layout->addWidget(message);
        auto *buttons=new QHBoxLayout;buttons->addStretch();cancel=new QPushButton(T("Cancel"));save=new QPushButton(QIcon(":/icons/save.png"), T("Save changes"));save->setIconSize(QSize(16, 16));save->setObjectName("primary");save->setEnabled(false);buttons->addWidget(cancel);buttons->addWidget(save);layout->addLayout(buttons);
        process=new QProcess(this);
        connect(process,&QProcess::readyReadStandardOutput,this,[this]{output+=process->readAllStandardOutput();});
        connect(process,&QProcess::readyReadStandardError,this,[this]{process->readAllStandardError();});
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,&SettingsDialog::finish);
        connect(process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart)showStatus(installing ? "Could not download Proton." : (saving ? "Could not save settings." : "Could not load settings."));});
        connect(download,&QPushButton::clicked,this,[this]{
            saving=false;installing=true;downloadProgress->show();
            message->setText(T("Downloading and checking Proton and its runtime … This may take several minutes."));
            run({"install_proton","--proton",downloadVersion->currentData().toString()});
        });
        connect(cancel,&QPushButton::clicked,this,&SettingsDialog::reject);
        connect(browse,&QPushButton::clicked,this,[this]{
            QString current=prefix->text().trimmed();
            if(current.isEmpty()) current=defaultPrefix;
            QString startDir=(!current.isEmpty()&&QDir(current).exists())?current:QDir::homePath();
            QString dir=QFileDialog::getExistingDirectory(this,T("Select Proton / Wine directory"),startDir);
            if(!dir.isEmpty())prefix->setText(dir);
        });
        connect(save,&QPushButton::clicked,this,[this]{
            QString chosenProton=proton->currentData().toString();
            QString chosenPrefix=prefix->text().trimmed();
            if(chosenPrefix.isEmpty() && !defaultPrefix.isEmpty()) chosenPrefix = defaultPrefix;
            bool protonChanged=(!chosenProton.isEmpty() && chosenProton!=originalProton);
            bool prefixChanged=(!chosenPrefix.isEmpty() && !originalPrefix.isEmpty() && chosenPrefix!=originalPrefix);
            if(protonChanged || prefixChanged) {
                QString warnTitle = (protonChanged && !prefixChanged) ? T("Change Proton version?") : T("Change installation directory?");
                QString warnText = (protonChanged && !prefixChanged)
                    ? T("Make sure all Windows apps are closed before continuing. The existing Windows environment will be kept.")
                    : T("Make sure all Windows apps are closed before continuing. If you moved your existing Windows environment, existing apps and data will be loaded from the new location.");
                QMessageBox box(QMessageBox::Question,warnTitle,warnText,QMessageBox::NoButton,this);
                auto *no=box.addButton(T("Cancel"),QMessageBox::RejectRole);auto *yes=box.addButton(T("Continue"),QMessageBox::AcceptRole);box.setDefaultButton(no);box.exec();if(box.clickedButton()!=yes)return;
            }
            saving=true;message->setText(T("Saving settings …"));
            QStringList args{"configure"};
            args << "--proton" << chosenProton;
            if(!chosenPrefix.isEmpty()) args << "--prefix" << chosenPrefix;
            run(args);
        });
        run({"settings"});
    }
};
