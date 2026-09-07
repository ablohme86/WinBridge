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

#include <QtWidgets>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QDesktopServices>
#include "i18n.h"
#include "settings.h"

class ClickableWidget : public QWidget {
    QListWidgetItem *item;
    QListWidget *list;
public:
    ClickableWidget(QListWidgetItem *i, QListWidget *l, QWidget *parent = nullptr)
        : QWidget(parent), item(i), list(l) {}
protected:
    void mousePressEvent(QMouseEvent *event) override {
        list->setCurrentItem(item);
        QWidget::mousePressEvent(event);
    }
};

class Manager : public QWidget {
    QListWidget *list;
    QLineEdit *search;
    QLabel *count, *engine, *status, *detailName, *detailText, *badge, *empty, *prefixLabel, *runningCountLabel;
    QPushButton *removeButton, *refreshButton, *folderButton, *configureButton, *killButton;
    QPushButton *tabAll = nullptr, *tabRunning = nullptr;
    QString currentFilter = "all";
    QProgressBar *progress;
    QProcess *process, *pollProcess;
    QTimer *pollTimer;
    QString backend, prefix, selectedKey, pendingAction;
    QByteArray output;
    bool busy = false;
    bool screenshot = false;
    QJsonArray programs;
    QSet<QString> expandedKeys;

    QLabel *label(const QString &text, const char *name = nullptr) {
        auto *l = new QLabel(text);
        l->setTextFormat(Qt::PlainText);
        if (name) l->setObjectName(name);
        return l;
    }
    QPushButton *button(const QString &text, const char *name = nullptr) {
        auto *b = new QPushButton(text);
        b->setCursor(Qt::PointingHandCursor);
        if (name) b->setObjectName(name);
        return b;
    }
    void showAppIcon(QLabel *target, const QString &path, const QString &fallback, int size) {
        target->clear();
        QPixmap pixmap;
        if (!path.isEmpty() && QFile::exists(path)) {
            pixmap.load(path);
        } else if (!path.isEmpty()) {
            QIcon themeIcon = QIcon::fromTheme(path);
            if (!themeIcon.isNull()) {
                const qreal ratio = target->devicePixelRatioF();
                pixmap = themeIcon.pixmap(qRound(size * ratio), qRound(size * ratio));
            }
        }
        if (pixmap.isNull()) {
            target->setText(fallback);
            return;
        }
        const qreal ratio = target->devicePixelRatioF();
        QPixmap scaled = pixmap.scaled(qRound(size * ratio), qRound(size * ratio), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(ratio);
        target->setPixmap(scaled);
    }
    void updateKillButtonState() {
        bool isRunning = false;
        for (const auto &val : programs) {
            auto obj = val.toObject();
            if (obj["key"].toString() == selectedKey) {
                isRunning = obj["running"].toBool();
                break;
            }
        }
        if (killButton) {
            killButton->setEnabled(!busy && !selectedKey.isEmpty() && isRunning);
        }
    }
    void setBusy(bool value) {
        busy = value;
        progress->setVisible(value);
        refreshButton->setEnabled(!value);
        configureButton->setEnabled(!value);
        removeButton->setEnabled(!value && !selectedKey.isEmpty());
        updateKillButtonState();
    }
    void executeBackend(QProcess *proc, const QStringList &args) {
        if (backend.endsWith(".py")) {
            proc->start("/usr/bin/python3", QStringList{backend} + args);
        } else {
            proc->start(backend, args);
        }
    }
    void request(const QString &action, const QString &key = {}) {
        if (busy) return;
        pendingAction = action;
        output.clear();
        setBusy(true);
        status->setText(action == "list" ? T("Loading your library …") : T("The uninstall wizard is running. Follow the instructions in its window."));
        QStringList args{action};
        if (!key.isEmpty()) args << "--key" << key;
        executeBackend(process, args);
    }
    void confirmAndKill(const QString &key, const QString &name) {
        if (busy) return;
        QMessageBox box(QMessageBox::Question, T("Kill running app"),
                        T("Are you sure you want to forcibly kill %1?").arg(name),
                        QMessageBox::NoButton, this);
        auto *cancel = box.addButton(T("Cancel"), QMessageBox::RejectRole);
        auto *yes = box.addButton(T("Kill"), QMessageBox::AcceptRole);
        box.setDefaultButton(cancel);
        box.exec();
        if (box.clickedButton() == yes) {
            killApp(key);
        }
    }
    void killApp(const QString &key) {
        if (busy) return;
        setBusy(true);
        status->setText(T("Killing app …"));
        auto *p = new QProcess(this);
        QStringList args{"kill", "--key", key};
        connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, p, key](int code, QProcess::ExitStatus) {
            setBusy(false);
            QByteArray resData = p->readAllStandardOutput();
            auto doc = QJsonDocument::fromJson(resData);
            if (code == 0 && doc.isObject() && doc.object()["killed"].toBool()) {
                status->setText(T("App terminated."));
                for (int i = 0; i < programs.size(); ++i) {
                    auto obj = programs[i].toObject();
                    if (obj["key"].toString() == key) {
                        obj["running"] = false;
                        programs[i] = obj;
                        break;
                    }
                }
                render();
            } else {
                status->setText(T("Could not terminate app."));
            }
            p->deleteLater();
            pollRunning();
        });
        executeBackend(p, args);
    }
    void pollRunning() {
        if (busy || !pollProcess || pollProcess->state() != QProcess::NotRunning || backend.isEmpty() || programs.isEmpty()) return;
        executeBackend(pollProcess, QStringList{"running"});
    }
    void toggleShortcut(const QString &id, const QString &target, bool enabled) {
        status->setText(T("Updating shortcut …"));
        auto *p = new QProcess(this);
        QStringList args{"toggle_shortcut", "--id", id, "--" + target, enabled ? "1" : "0"};
        connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, p](int code, QProcess::ExitStatus) {
            if (code == 0) {
                status->setText(T("Shortcut updated."));
            } else {
                status->setText(T("Could not update shortcut."));
            }
            p->deleteLater();
        });
        executeBackend(p, args);
    }
    void render() {
        QString previous = selectedKey;
        list->clear();
        selectedKey.clear();
        int visible = 0;
        int runningCount = 0;
        for (const auto &value : programs) {
            if (value.toObject()["running"].toBool()) runningCount++;
        }

        if (tabAll) tabAll->setText(T("All (%1)").arg(programs.size()));
        if (tabRunning) tabRunning->setText(T("Active (%1)").arg(runningCount));

        for (const auto &value : programs) {
            auto p = value.toObject();
            QString name = p["name"].toString(), key = p["key"].toString();
            bool isRunning = p["running"].toBool();

            if (currentFilter == "running" && !isRunning) continue;
            if (!name.contains(search->text(), Qt::CaseInsensitive)) continue;

            auto *item = new QListWidgetItem(list);
            item->setData(Qt::UserRole, key);
            item->setData(Qt::UserRole + 1, name);
            item->setData(Qt::UserRole + 2, p["icon"].toString());

            auto *container = new QWidget;
            auto *mainLayout = new QVBoxLayout(container);
            mainLayout->setContentsMargins(0, 0, 0, 0);
            mainLayout->setSpacing(0);

            auto *header = new ClickableWidget(item, list);
            header->setObjectName("itemCard");
            auto *headerLayout = new QHBoxLayout(header);
            headerLayout->setContentsMargins(18, 12, 18, 12);
            headerLayout->setSpacing(14);

            auto *icon = label(name.left(1).toUpper(), "appIcon");
            icon->setAlignment(Qt::AlignCenter);
            icon->setFixedSize(46, 46);
            showAppIcon(icon, p["icon"].toString(), name.left(1).toUpper(), 38);
            headerLayout->addWidget(icon);

            auto *texts = new QVBoxLayout;
            texts->setSpacing(4);
            auto *title = label(name, "appName");
            title->setToolTip(name);
            texts->addWidget(title);
            texts->addWidget(label(T("Windows app  ·  Shared environment"), "muted"));
            headerLayout->addLayout(texts, 1);

            if (isRunning) {
                auto *runBadge = label(T("●  Running"), "running");
                headerLayout->addWidget(runBadge);
                auto *killBtn = button(T("Kill"), "itemKill");
                killBtn->setObjectName("itemKill");
                killBtn->setToolTip(T("Forcibly terminate running app"));
                headerLayout->addWidget(killBtn);
                connect(killBtn, &QPushButton::clicked, this, [this, key, name, item] {
                    list->setCurrentItem(item);
                    confirmAndKill(key, name);
                });
            } else {
                headerLayout->addWidget(label(T("Installed"), "installed"));
            }

            QJsonArray shortcuts = p["shortcuts"].toArray();
            QFrame *panel = nullptr;
            QPushButton *expandBtn = nullptr;

            if (shortcuts.isEmpty()) {
                expandBtn = button(T("No shortcuts"), "shortcutToggle");
                expandBtn->setEnabled(false);
                headerLayout->addWidget(expandBtn);
            } else {
                bool isExpanded = expandedKeys.contains(key);
                expandBtn = button(T("Shortcuts (%1) %2").arg(shortcuts.size()).arg(isExpanded ? "▾" : "▸"), "shortcutToggle");
                headerLayout->addWidget(expandBtn);

                panel = new QFrame;
                panel->setObjectName("shortcutsPanel");
                auto *panelLayout = new QVBoxLayout(panel);
                panelLayout->setContentsMargins(76, 6, 20, 14);
                panelLayout->setSpacing(8);

                for (const auto &scVal : shortcuts) {
                    auto sc = scVal.toObject();
                    QString scId = sc["id"].toString();
                    QString scName = sc["name"].toString();
                    QString scIcon = sc["icon"].toString();
                    bool onDesktop = sc["desktop"].toBool();
                    bool inMenu = sc["menu"].toBool();

                    auto *scRow = new QHBoxLayout;
                    scRow->setSpacing(10);

                    auto *miniIcon = label(scName.left(1).toUpper(), "miniAppIcon");
                    miniIcon->setAlignment(Qt::AlignCenter);
                    miniIcon->setFixedSize(24, 24);
                    showAppIcon(miniIcon, scIcon, scName.left(1).toUpper(), 18);
                    scRow->addWidget(miniIcon);

                    auto *scTitle = label(scName, "shortcutName");
                    scTitle->setToolTip(scName);
                    scRow->addWidget(scTitle, 1);

                    auto *cbDesktop = new QCheckBox(T("Desktop"));
                    cbDesktop->setObjectName("cbDesktop");
                    cbDesktop->setChecked(onDesktop);
                    scRow->addWidget(cbDesktop);

                    auto *cbMenu = new QCheckBox(T("Start menu"));
                    cbMenu->setObjectName("cbMenu");
                    cbMenu->setChecked(inMenu);
                    scRow->addWidget(cbMenu);

                    connect(cbDesktop, &QCheckBox::toggled, this, [this, scId, item](bool checked) {
                        list->setCurrentItem(item);
                        toggleShortcut(scId, "desktop", checked);
                    });
                    connect(cbMenu, &QCheckBox::toggled, this, [this, scId, item](bool checked) {
                        list->setCurrentItem(item);
                        toggleShortcut(scId, "menu", checked);
                    });

                    panelLayout->addLayout(scRow);
                }

                panel->setVisible(isExpanded);

                connect(expandBtn, &QPushButton::clicked, this, [this, key, expandBtn, panel, item, container, shortcuts] {
                    list->setCurrentItem(item);
                    bool nowExpanded = !panel->isVisible();
                    panel->setVisible(nowExpanded);
                    if (nowExpanded) expandedKeys.insert(key);
                    else expandedKeys.remove(key);
                    expandBtn->setText(T("Shortcuts (%1) %2").arg(shortcuts.size()).arg(nowExpanded ? "▾" : "▸"));
                    container->adjustSize();
                    item->setSizeHint(container->sizeHint());
                });
            }

            mainLayout->addWidget(header);
            if (panel) mainLayout->addWidget(panel);

            container->adjustSize();
            item->setSizeHint(container->sizeHint());
            list->setItemWidget(item, container);
            if (key == previous) list->setCurrentItem(item);
            visible++;
        }
        count->setText(QString::number(programs.size()));
        empty->setVisible(visible == 0);
        empty->setText(programs.isEmpty() ? T("No apps installed yet\n\nOpen an .exe file with WinBridge to get started.\nPortable apps without a registered installation are not listed here.") : T("No apps match your search."));
        list->setVisible(visible > 0);
        updateSelection();
    }
    void updateSelection() {
        auto *item = list->currentItem();
        selectedKey = item ? item->data(Qt::UserRole).toString() : QString();
        QString name = item ? item->data(Qt::UserRole + 1).toString() : T("Select an app");
        detailName->setText(name);
        showAppIcon(badge, item ? item->data(Qt::UserRole + 2).toString() : QString(), item ? name.left(1).toUpper() : "W", 64);
        bool isRunning = false;
        if (item) {
            for (const auto &val : programs) {
                auto obj = val.toObject();
                if (obj["key"].toString() == selectedKey) {
                    isRunning = obj["running"].toBool();
                    break;
                }
            }
        }
        detailText->setText(item ? (isRunning ? (T("Active Now") + "  ·  " + T("Installed in your shared Windows environment. Uninstalling opens the app’s own wizard.")) : T("Installed in your shared Windows environment. Uninstalling opens the app’s own wizard.")) : T("Your Windows apps, all in one place."));
        removeButton->setEnabled(item && !busy);
        updateKillButtonState();
    }
    void finished(int code, QProcess::ExitStatus exitStatus) {
        output += process->readAllStandardOutput();
        QJsonParseError parseError;
        auto document = QJsonDocument::fromJson(output, &parseError);
        setBusy(false);
        auto result = document.object();
        if (exitStatus != QProcess::NormalExit || code != 0 || parseError.error != QJsonParseError::NoError || !document.isObject() || result.contains("error")) {
            QString error = result["error"].toString();
            if (error.isEmpty()) error = T("Could not read your Windows environment. Try refreshing the library.");
            status->setText(T(error));
            return;
        }
        prefix = result["prefix"].toString();
        folderButton->setEnabled(QDir(prefix + "/pfx/drive_c").exists());
        folderButton->setToolTip(prefix + "/pfx/drive_c");
        if (prefixLabel) {
            QString displayPrefix = prefix;
            if (displayPrefix.startsWith(QDir::homePath())) {
                displayPrefix.replace(0, QDir::homePath().length(), "~");
            }
            prefixLabel->setText(displayPrefix);
            prefixLabel->setToolTip(prefix);
        }
        engine->setText(T(result["proton"].toString()));
        engine->setToolTip(result["proton"].toString());
        programs = result["programs"].toArray();
        render();
        if (pendingAction == "uninstall") {
            status->setText(result["removed"].toBool() ? T("The app has been removed from the installed programs list.") : T("The app is still registered. Uninstallation may have been cancelled or may still be running."));
        } else {
            status->setText(result["ready"].toBool() ? T("Updated just now · Ready") : T("Your environment is created when you first open an .exe file with WinBridge."));
        }
    }
protected:
    void closeEvent(QCloseEvent *event) override {
        if (busy) {
            QMessageBox::information(this, T("Operation in progress"), T("Please wait for the current operation to finish before closing Manager."));
            event->ignore();
        } else event->accept();
    }
    void showSettings() {
        SettingsDialog dialog(backend, this);
        if (dialog.exec() == QDialog::Accepted) {
            if (dialog.languageChanged()) qApp->exit(42);
            else request("list");
        }
    }
public:
    Manager(const QString &backendPath, bool preview = false, bool demo = false) : backend(backendPath), screenshot(preview) {
        setWindowTitle("WinBridge Manager");
        setWindowIcon(QIcon(":/assets/winbridge.png"));
        resize(1200, 780);
        setMinimumSize(1020, 680);
        setStyleSheet(R"(
            QWidget { background: #0c0f17; color: #e2e8f0; font-family: 'Noto Sans', 'Segoe UI', sans-serif; font-size: 13px; }
            QFrame#topNav { background: #111520; border-bottom: 1px solid #1c2333; }
            QLabel { background: transparent; }
            QLabel#brand { font-size: 20px; font-weight: 800; color: #ffffff; letter-spacing: -0.5px; }
            QLabel#brandSub { font-size: 10px; font-weight: 700; color: #8595b3; letter-spacing: 1.5px; }
            QLabel#eyebrow { color: #818ea8; font-size: 10px; font-weight: 700; letter-spacing: 1.5px; }
            QLabel#heading { font-size: 28px; font-weight: 800; letter-spacing: -0.5px; color: #ffffff; }
            QLabel#muted { color: #8290ab; font-size: 12px; }
            QLabel#count { font-size: 26px; font-weight: 800; color: #ffffff; }
            QLabel#engine { font-size: 15px; font-weight: 700; color: #d8b4fe; }
            QLabel#envPath { font-size: 13px; font-weight: 600; color: #93c5fd; }
            QFrame#heroBanner { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #131724, stop:0.5 #161b2a, stop:1 #131724); border: 1px solid #202738; border-radius: 14px; }
            QFrame#statCard { background: #0e121c; border: 1px solid #1c2333; border-radius: 10px; padding: 4px; }
            QFrame#detail { background: #121622; border: 1px solid #1e2638; border-radius: 16px; }
            QLabel#appIcon { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #2b274c, stop:1 #1a1833); color: #c4b5fd; border: 1px solid #3d376b; border-radius: 12px; font-weight: 800; font-size: 20px; }
            QLabel#badge { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #35305c, stop:1 #1e1b38); color: #c4b5fd; border: 2px solid #54498a; border-radius: 18px; font-size: 38px; font-weight: 800; }
            QLabel#appName { font-size: 15px; font-weight: 700; color: #f8fafc; }
            QLabel#installed { background: #0f2321; color: #34d399; border: 1px solid #164e43; border-radius: 6px; padding: 3px 9px; font-size: 11px; font-weight: 700; }
            QLabel#running { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #064e3b, stop:1 #065f46); color: #4ade80; border: 1px solid #10b981; border-radius: 6px; padding: 3px 9px; font-size: 11px; font-weight: 800; }
            QLabel#detailName { font-size: 20px; font-weight: 800; color: #ffffff; letter-spacing: -0.5px; }
            QPushButton { background: #181d2a; border: 1px solid #283144; border-radius: 9px; padding: 8px 14px; font-weight: 600; font-size: 12px; color: #e2e8f0; }
            QPushButton:hover { background: #22293b; border-color: #3e4d69; color: #ffffff; }
            QPushButton:focus { border-color: #8b5cf6; }
            QPushButton:disabled { color: #4c576e; background: #11141e; border-color: #1a1f2c; }
            QPushButton#primary { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #7c3aed, stop:1 #6d28d9); border: 1px solid #8b5cf6; color: #ffffff; font-weight: 700; }
            QPushButton#primary:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #8b5cf6, stop:1 #7c3aed); border-color: #a78bfa; }
            QPushButton#danger { background: #26151e; color: #fb7185; border: 1px solid #4c2032; font-weight: 600; }
            QPushButton#danger:hover { background: #38192a; color: #ffffff; border-color: #792849; }
            QPushButton#danger:disabled { background: #11141e; color: #4c576e; border-color: #1a1f2c; }
            QPushButton#itemKill, QPushButton#killAppButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4c0519, stop:1 #350715); border: 1px solid #e11d48; border-radius: 7px; padding: 4px 11px; color: #fda4af; font-size: 11px; font-weight: 700; }
            QPushButton#itemKill:hover, QPushButton#killAppButton:hover { background: #881337; border-color: #f43f5e; color: #ffffff; }
            QPushButton#killAppButton:disabled { background: #11141e; border-color: #1a1f2c; color: #4c576e; }
            QPushButton#filterTab { background: transparent; border: 1px solid transparent; border-radius: 8px; padding: 6px 14px; font-weight: 700; font-size: 12px; color: #8290ab; }
            QPushButton#filterTab:hover { background: #181d2a; color: #f1f5f9; }
            QPushButton#filterTab:checked { background: #221f3d; border: 1px solid #5c43c2; color: #c4b5fd; }
            QPushButton#shortcutToggle { background: #141824; border: 1px solid #232a3b; border-radius: 7px; padding: 4px 10px; font-size: 11px; font-weight: 600; color: #8e9bb5; }
            QPushButton#shortcutToggle:hover { background: #1c2233; color: #f8fafc; border-color: #384560; }
            QPushButton#shortcutToggle:disabled { color: #475569; background: #10131c; border-color: #181c26; }
            QFrame#shortcutsPanel { background: #0c0f16; border-top: 1px solid #1a202d; border-radius: 0 0 14px 14px; padding: 8px 16px 12px 16px; }
            QLabel#miniAppIcon { background: #221e3a; color: #c4b5fd; border-radius: 6px; font-weight: 700; font-size: 11px; }
            QLabel#shortcutName { font-size: 12px; font-weight: 600; color: #e2e8f0; }
            QCheckBox { color: #8290ab; spacing: 6px; font-size: 11px; font-weight: 600; }
            QCheckBox:hover { color: #e2e8f0; }
            QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid #2d374d; border-radius: 4px; background: #141824; }
            QCheckBox::indicator:hover { border-color: #8b5cf6; }
            QCheckBox::indicator:checked { background: #7c3aed; border-color: #a78bfa; }
            QLineEdit#search { background: #121622; border: 1px solid #222a3a; border-radius: 10px; padding: 8px 14px; selection-background-color: #6d28d9; color: #f8fafc; }
            QLineEdit#search:focus { border-color: #8b5cf6; background: #161b29; }
            QListWidget#programList { background: transparent; border: 0; outline: 0; }
            QListWidget#programList::item { background: #131722; border: 1px solid #1e2637; border-radius: 13px; margin-bottom: 8px; }
            QListWidget#programList::item:selected { background: #211e3b; border: 1px solid #7c5cfc; }
            QListWidget#programList::item:hover { border-color: #37435e; }
            QScrollBar:vertical { background: #0c0f17; width: 6px; border-radius: 3px; }
            QScrollBar::handle:vertical { background: #2a3346; min-height: 25px; border-radius: 3px; }
            QScrollBar::handle:vertical:hover { background: #475775; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
            QProgressBar { border: 0; background: #161b28; max-height: 2px; }
            QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #7c3aed, stop:1 #38bdf8); }
        )");

        auto *windowLayout = new QVBoxLayout(this);
        windowLayout->setContentsMargins(0, 0, 0, 0);
        windowLayout->setSpacing(0);

        // 1. Top Navigation Bar
        auto *topNav = new QFrame; topNav->setObjectName("topNav");
        auto *navLayout = new QHBoxLayout(topNav);
        navLayout->setContentsMargins(24, 14, 24, 14);
        navLayout->setSpacing(16);

        auto *brandLogo = new QLabel;
        brandLogo->setPixmap(QPixmap(":/assets/winbridge.png").scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        navLayout->addWidget(brandLogo);

        auto *brandTitles = new QVBoxLayout; brandTitles->setSpacing(1);
        auto *brandTitle = label("WinBridge", "brand");
        auto *brandSub = label("PRO APP MANAGER", "brandSub");
        brandTitles->addWidget(brandTitle); brandTitles->addWidget(brandSub);
        navLayout->addLayout(brandTitles);

        navLayout->addSpacing(16);

        // Filter pills (All / Active)
        tabAll = button(T("All (%1)").arg(0), "filterTab");
        tabAll->setCheckable(true); tabAll->setChecked(true);
        tabRunning = button(T("Active (%1)").arg(0), "filterTab");
        tabRunning->setCheckable(true);
        navLayout->addWidget(tabAll);
        navLayout->addWidget(tabRunning);

        connect(tabAll, &QPushButton::clicked, this, [this] {
            tabAll->setChecked(true);
            tabRunning->setChecked(false);
            currentFilter = "all";
            render();
        });
        connect(tabRunning, &QPushButton::clicked, this, [this] {
            tabRunning->setChecked(true);
            tabAll->setChecked(false);
            currentFilter = "running";
            render();
        });

        navLayout->addStretch();

        search = new QLineEdit;
        search->setObjectName("search");
        search->setPlaceholderText(T("Search for an app …"));
        search->setClearButtonEnabled(true);
        search->setFixedWidth(240);
        navLayout->addWidget(search);

        refreshButton = button(T("↻  Refresh"));
        navLayout->addWidget(refreshButton);

        folderButton = button(T("Open C: Drive"));
        folderButton->setEnabled(false);
        navLayout->addWidget(folderButton);

        configureButton = button(T("Settings"));
        navLayout->addWidget(configureButton);

        windowLayout->addWidget(topNav);

        // 2. Main Content Canvas
        auto *contentArea = new QVBoxLayout;
        contentArea->setContentsMargins(24, 18, 24, 16);
        contentArea->setSpacing(14);
        windowLayout->addLayout(contentArea, 1);

        // Hero System Deck Banner
        auto *heroBanner = new QFrame; heroBanner->setObjectName("heroBanner");
        auto *heroLayout = new QHBoxLayout(heroBanner);
        heroLayout->setContentsMargins(18, 12, 18, 12);
        heroLayout->setSpacing(16);

        auto makeDeckCard = [&](const QString &eyebrowText, QLabel *valueWidget) {
            auto *card = new QFrame; card->setObjectName("statCard");
            auto *cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(16, 10, 16, 10);
            cardLayout->setSpacing(3);
            cardLayout->addWidget(label(eyebrowText, "eyebrow"));
            cardLayout->addWidget(valueWidget);
            return card;
        };

        count = label("0", "count");
        engine = label(T("Loading …"), "engine"); engine->setWordWrap(true);
        prefixLabel = label("~/.local/share/winbridge/shared", "envPath"); prefixLabel->setWordWrap(true);

        heroLayout->addWidget(makeDeckCard(T("INSTALLED APPS"), count), 1);
        heroLayout->addWidget(makeDeckCard(T("PROTON VERSION"), engine), 2);
        heroLayout->addWidget(makeDeckCard(T("ENVIRONMENT"), prefixLabel), 2);
        contentArea->addWidget(heroBanner);

        // Main Body Split (Library on left, Detail on right)
        auto *body = new QHBoxLayout; body->setSpacing(18);

        auto *left = new QVBoxLayout; left->setSpacing(0);
        list = new QListWidget; list->setObjectName("programList");
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        left->addWidget(list);
        empty = label(T("Loading your apps …"), "muted");
        empty->setAlignment(Qt::AlignCenter); empty->setWordWrap(true);
        left->addWidget(empty, 1);
        body->addLayout(left, 1);

        // Right Detail Inspector Panel
        auto *detail = new QFrame; detail->setObjectName("detail"); detail->setFixedWidth(270);
        auto *dl = new QVBoxLayout(detail); dl->setContentsMargins(22, 24, 22, 22); dl->setSpacing(14);

        badge = label("W", "badge"); badge->setAlignment(Qt::AlignCenter); badge->setFixedSize(76, 76);
        dl->addWidget(badge, 0, Qt::AlignCenter);

        detailName = label(T("Select an app"), "detailName"); detailName->setWordWrap(true); detailName->setAlignment(Qt::AlignCenter);
        dl->addWidget(detailName);

        detailText = label(T("Your Windows apps, all in one place."), "muted"); detailText->setWordWrap(true); detailText->setAlignment(Qt::AlignCenter);
        dl->addWidget(detailText);

        dl->addStretch();

        killButton = button(T("Kill app"), "killAppButton"); killButton->setEnabled(false); dl->addWidget(killButton);
        removeButton = button(T("Uninstall app"), "danger"); removeButton->setEnabled(false); dl->addWidget(removeButton);

        body->addWidget(detail);
        contentArea->addLayout(body, 1);

        // Footer status bar & progress
        progress = new QProgressBar; progress->setRange(0, 0); progress->setTextVisible(false); progress->hide();
        contentArea->addWidget(progress);

        status = label(T("Ready"), "muted"); status->setWordWrap(true);
        contentArea->addWidget(status);

        process = new QProcess(this);
        connect(process, &QProcess::readyReadStandardOutput, this, [this] { output += process->readAllStandardOutput(); });
        connect(process, &QProcess::readyReadStandardError, this, [this] { process->readAllStandardError(); });
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &Manager::finished);
        connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) {
                setBusy(false);
                status->setText(T("Could not start the WinBridge integration: ") + process->errorString());
            }
        });
        connect(search, &QLineEdit::textChanged, this, [this] { render(); });
        connect(list, &QListWidget::currentItemChanged, this, [this] { updateSelection(); });
        connect(refreshButton, &QPushButton::clicked, this, [this] { request("list"); });
        connect(folderButton, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(prefix + "/pfx/drive_c")); });
        connect(configureButton, &QPushButton::clicked, this, [this] { showSettings(); });
        connect(killButton, &QPushButton::clicked, this, [this] {
            if (selectedKey.isEmpty() || busy) return;
            confirmAndKill(selectedKey, detailName->text());
        });
        connect(removeButton, &QPushButton::clicked, this, [this] {
            if (selectedKey.isEmpty() || busy) return;
            QMessageBox box(QMessageBox::Question, T("Uninstall app"), T("Uninstall “%1”?\n\nThe app’s own uninstall wizard will open.").arg(detailName->text()), QMessageBox::NoButton, this);
            auto *cancel = box.addButton(T("Cancel"), QMessageBox::RejectRole);
            auto *yes = box.addButton(T("Uninstall"), QMessageBox::AcceptRole);
            box.setDefaultButton(cancel);
            box.exec();
            if (box.clickedButton() == yes) request("uninstall", selectedKey);
        });

        pollProcess = new QProcess(this);
        connect(pollProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus) {
            if (code == 0 && !busy) {
                auto doc = QJsonDocument::fromJson(pollProcess->readAllStandardOutput());
                if (doc.isObject()) {
                    auto runningMap = doc.object()["running"].toObject();
                    bool changed = false;
                    for (int i = 0; i < programs.size(); ++i) {
                        auto obj = programs[i].toObject();
                        QString k = obj["key"].toString();
                        if (runningMap.contains(k)) {
                            bool isRun = runningMap[k].toBool();
                            if (obj["running"].toBool() != isRun) {
                                obj["running"] = isRun;
                                programs[i] = obj;
                                changed = true;
                            }
                        }
                    }
                    if (changed) render();
                }
            }
        });
        pollTimer = new QTimer(this);
        pollTimer->setInterval(3000);
        connect(pollTimer, &QTimer::timeout, this, [this] { pollRunning(); });
        if (!screenshot && !demo) {
            pollTimer->start();
            QTimer::singleShot(0, this, [this] { request("list"); });
        } else if (demo) {
            engine->setText("GE-Proton9-25");
            programs = {
                QJsonObject{
                    {"key", "notepad-plus-plus"},
                    {"name", "Notepad++"},
                    {"running", true},
                    {"pids", QJsonArray{14201}},
                    {"shortcuts", QJsonArray{
                        QJsonObject{{"id", "notepad-plus-plus"}, {"name", "Notepad++"}, {"desktop", true}, {"menu", true}}
                    }}
                },
                QJsonObject{
                    {"key", "7zip"},
                    {"name", "7-Zip File Manager"},
                    {"running", false},
                    {"pids", QJsonArray{}},
                    {"shortcuts", QJsonArray{
                        QJsonObject{{"id", "7zip-fm"}, {"name", "7-Zip File Manager"}, {"desktop", false}, {"menu", true}}
                    }}
                },
                QJsonObject{
                    {"key", "affinity-photo"},
                    {"name", "Affinity Photo 2"},
                    {"running", false},
                    {"pids", QJsonArray{}},
                    {"shortcuts", QJsonArray{
                        QJsonObject{{"id", "affinity-photo-2"}, {"name", "Affinity Photo 2"}, {"desktop", true}, {"menu", true}}
                    }}
                }
            };
            expandedKeys.insert("notepad-plus-plus");
            render();
            if (list->count() > 0) {
                list->setCurrentRow(0);
            }
        } else {
            engine->setText(T("Not selected"));
            render();
            status->setText(T("Your environment is created when you first open an .exe file with WinBridge."));
        }
    }
};

#ifndef WINBRIDGE_MANAGER_TEST
int main(int argc, char **argv) {
    QApplication app(argc,argv);
    app.setWindowIcon(QIcon(":/assets/winbridge.png"));
    app.setApplicationName("WinBridge Manager"); app.setDesktopFileName("winbridge-manager");
    QCommandLineParser parser;parser.addHelpOption();parser.addOption({"backend","Path to the WinBridge integration module","path"});parser.addOption({"screenshot","Render an empty-state preview and exit","path"});parser.addOption({"demo","Populate with demo data for preview"});parser.addOption({"snapshot","Render a live library snapshot and exit","path"});parser.process(app);
    QString backend=parser.value("backend");
    if(backend.isEmpty()) {
        QDir bin(QCoreApplication::applicationDirPath());
        QString standardBackend = QStandardPaths::findExecutable("winbridge-backend");
        QStringList candidates{
            bin.filePath("winbridge-backend"),
            bin.filePath("../winbridge-backend"),
            standardBackend,
            QDir::homePath() + "/.local/bin/winbridge-backend",
            QDir::homePath() + "/.local/share/winbridge/winbridge-backend",
            "/usr/local/bin/winbridge-backend",
            "/usr/bin/winbridge-backend",
            bin.filePath("manager_backend.py"),
            bin.filePath("../../manager_backend.py"),
            bin.filePath("../lib/winbridge/manager_backend.py")
        };
        for(const auto &candidate:candidates)if(!candidate.isEmpty() && QFileInfo::exists(candidate)){backend=QFileInfo(candidate).absoluteFilePath();break;}
    }
    int exitCode;
    do {
    I18n::load(QSettings("WinBridge", "Manager").value("language", "en-US").toString());
    Manager window(backend,parser.isSet("screenshot"),parser.isSet("demo"));window.show();
    if(parser.isSet("screenshot"))QTimer::singleShot(500,&app,[&]{bool ok=window.grab().save(parser.value("screenshot"));app.exit(ok?0:1);});
    else if(parser.isSet("snapshot"))QTimer::singleShot(1800,&app,[&]{bool ok=window.grab().save(parser.value("snapshot"));app.exit(ok?0:1);});
    exitCode = app.exec();
    } while (exitCode == 42);
    return exitCode;
}

#endif
