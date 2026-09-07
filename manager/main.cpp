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
    QLabel *count, *engine, *status, *detailName, *detailText, *badge, *empty;
    QPushButton *removeButton, *refreshButton, *folderButton, *configureButton;
    QProgressBar *progress;
    QProcess *process;
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
        QPixmap pixmap(path);
        if (pixmap.isNull()) {
            target->setText(fallback);
            return;
        }
        const qreal ratio = target->devicePixelRatioF();
        QPixmap scaled = pixmap.scaled(qRound(size * ratio), qRound(size * ratio), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(ratio);
        target->setPixmap(scaled);
    }
    void setBusy(bool value) {
        busy = value;
        progress->setVisible(value);
        refreshButton->setEnabled(!value);
        configureButton->setEnabled(!value);
        removeButton->setEnabled(!value && !selectedKey.isEmpty());
    }
    void request(const QString &action, const QString &key = {}) {
        if (busy) return;
        pendingAction = action;
        output.clear();
        setBusy(true);
        status->setText(action == "list" ? T("Loading your library …") : T("The uninstall wizard is running. Follow the instructions in its window."));
        QStringList args{backend, action};
        if (!key.isEmpty()) args << "--key" << key;
        process->start("/usr/bin/python3", args);
    }
    void toggleShortcut(const QString &id, const QString &target, bool enabled) {
        status->setText(T("Updating shortcut …"));
        auto *p = new QProcess(this);
        QStringList args{backend, "toggle_shortcut", "--id", id, "--" + target, enabled ? "1" : "0"};
        connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, p](int code, QProcess::ExitStatus) {
            if (code == 0) {
                status->setText(T("Shortcut updated."));
            } else {
                status->setText(T("Could not update shortcut."));
            }
            p->deleteLater();
        });
        p->start("/usr/bin/python3", args);
    }
    void render() {
        QString previous = selectedKey;
        list->clear();
        selectedKey.clear();
        int visible = 0;
        for (const auto &value : programs) {
            auto p = value.toObject();
            QString name = p["name"].toString(), key = p["key"].toString();
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
            auto *headerLayout = new QHBoxLayout(header);
            headerLayout->setContentsMargins(18, 10, 20, 10);
            auto *icon = label(name.left(1).toUpper(), "appIcon");
            icon->setAlignment(Qt::AlignCenter);
            icon->setFixedSize(44, 44);
            showAppIcon(icon, p["icon"].toString(), name.left(1).toUpper(), 36);
            headerLayout->addWidget(icon);
            auto *texts = new QVBoxLayout;
            texts->setSpacing(5);
            auto *title = label(name, "appName");
            title->setToolTip(name);
            texts->addWidget(title);
            texts->addWidget(label(T("Windows app  ·  Shared environment"), "muted"));
            headerLayout->addLayout(texts, 1);
            headerLayout->addWidget(label(T("Installed"), "installed"));

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
                panelLayout->setContentsMargins(76, 4, 20, 12);
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
                    miniIcon->setFixedSize(22, 22);
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
        showAppIcon(badge, item ? item->data(Qt::UserRole + 2).toString() : QString(), item ? name.left(1).toUpper() : "W", 60);
        detailText->setText(item ? T("Installed in your shared Windows environment. Uninstalling opens the app’s own wizard.") : T("Your Windows apps, all in one place."));
        removeButton->setEnabled(item && !busy);
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
    Manager(const QString &backendPath, bool preview = false) : backend(backendPath), screenshot(preview) {
        setWindowTitle("WinBridge Manager");
        setWindowIcon(QIcon(":/assets/winbridge.png"));
        resize(1180, 760);
        setMinimumSize(1000, 660);
        setStyleSheet(R"(
            QWidget { background: #10131c; color: #e9edf6; font-family: 'Noto Sans'; font-size: 13px; }
            QFrame#sidebar { background: #151925; border-right: 1px solid #272c3e; }
            QLabel { background: transparent; }
            QLabel#brand { font-size: 24px; font-weight: 800; letter-spacing: -1px; }
            QLabel#eyebrow { color: #8994b2; font-size: 10px; font-weight: 700; letter-spacing: 2px; }
            QLabel#heading { font-size: 32px; font-weight: 800; letter-spacing: -1px; }
            QLabel#muted { color: #8e99b1; font-size: 12px; }
            QLabel#count { font-size: 30px; font-weight: 700; }
            QLabel#engine { font-size: 16px; font-weight: 600; }
            QFrame#card { background: #191e2c; border: 1px solid #2b3246; border-radius: 14px; }
            QFrame#detail { background: #151a26; border: 1px solid #2b3246; border-radius: 14px; }
            QLabel#appIcon, QLabel#badge { background: #303052; color: #c3b9ff; border-radius: 12px; font-weight: 700; font-size: 22px; }
            QLabel#badge { font-size: 36px; border-radius: 18px; }
            QLabel#appName { font-size: 14px; font-weight: 600; }
            QLabel#installed { color: #74d8b4; font-size: 11px; }
            QLabel#detailName { font-size: 20px; font-weight: 700; }
            QPushButton { background: #22293a; border: 1px solid #333d54; border-radius: 9px; padding: 11px 16px; font-weight: 600; }
            QPushButton:hover { background: #30394f; border-color: #6973a0; }
            QPushButton:focus { border: 1px solid #a899ff; }
            QPushButton:disabled { color: #58627c; background: #1a1f2d; border-color: #282e40; }
            QPushButton#nav { background: #2b2947; border: 1px solid #46406b; color: #cfc5ff; text-align: left; }
            QPushButton#primary { background: #8970ef; color: #ffffff; border: 0; }
            QPushButton#primary:hover { background: #a087ff; }
            QPushButton#danger { background: #34222e; color: #f9a0b7; border: 1px solid #5b3347; }
            QPushButton#danger:disabled { background: #1a1f2d; color: #58627c; border-color: #282e40; }
            QComboBox { background: #22293a; color: #e9edf6; border: 1px solid #3b4560; border-radius: 8px; padding: 12px; min-width: 180px; }
            QComboBox QAbstractItemView { background: #22293a; selection-background-color: #514274; }
            QLineEdit { background: #191e2c; border: 1px solid #30384e; border-radius: 10px; padding: 13px 16px; selection-background-color: #7461bd; }
            QLineEdit:focus { border-color: #a899ff; }
            QListWidget { background: transparent; border: 0; outline: 0; }
            QListWidget::item { background: #191e2c; border: 1px solid #2a3143; border-radius: 11px; margin-bottom: 8px; }
            QListWidget::item:selected { background: #292642; border: 1px solid #8a75d1; }
            QListWidget::item:hover { border-color: #5b617c; }
            QScrollBar:vertical { background: #151925; width: 8px; border-radius: 4px; }
            QScrollBar::handle:vertical { background: #3b445d; min-height: 30px; border-radius: 4px; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
            QProgressBar { border: 0; background: #252b3e; max-height: 3px; }
            QProgressBar::chunk { background: #a48bff; }
            QFrame#shortcutsPanel { background: #131722; border-top: 1px solid #232838; border-radius: 0 0 11px 11px; padding: 6px 12px 10px 12px; }
            QLabel#miniAppIcon { background: #2b2b48; color: #b8adff; border-radius: 6px; font-weight: 700; font-size: 11px; }
            QLabel#shortcutName { font-size: 12px; font-weight: 600; color: #d6dce9; }
            QCheckBox { color: #8e99b1; spacing: 6px; font-size: 11px; font-weight: 600; }
            QCheckBox:hover { color: #e9edf6; }
            QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid #38425d; border-radius: 4px; background: #191e2c; }
            QCheckBox::indicator:hover { border-color: #8970ef; }
            QCheckBox::indicator:checked { background: #8970ef; border-color: #a48bff; }
            QPushButton#shortcutToggle { background: #1f2536; border: 1px solid #333d54; border-radius: 7px; padding: 4px 10px; font-size: 11px; font-weight: 600; color: #a4b1cd; }
            QPushButton#shortcutToggle:hover { background: #2c354c; color: #ffffff; border-color: #6973a0; }
            QPushButton#shortcutToggle:disabled { color: #58627c; background: #171b26; border-color: #242938; }
        )");
        auto *outer = new QHBoxLayout(this); outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);
        auto *sidebar = new QFrame; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(214);
        auto *side = new QVBoxLayout(sidebar); side->setContentsMargins(24,32,24,26); side->setSpacing(18);
        auto *brandIcon = new QLabel; brandIcon->setPixmap(QPixmap(":/assets/winbridge.png").scaled(56,56,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        side->addWidget(brandIcon); side->addWidget(label("WinBridge", "brand"));
        side->addWidget(label("MANAGER", "eyebrow")); side->addSpacing(36);
        auto *nav = button(T("▦   Apps"), "nav"); side->addWidget(nav);
        side->addWidget(label(T("YOUR WORKSPACE"), "eyebrow"));
        auto *sideInfo = label(T("Windows apps.\nAt home on Linux."), "muted"); sideInfo->setWordWrap(true); side->addWidget(sideInfo);
        side->addStretch();
        side->addWidget(label(T("●  One shared environment"), "installed"));
        auto *foot = label(T("Your apps and data,\ntogether in one place."), "muted"); side->addWidget(foot);
        outer->addWidget(sidebar);
        auto *main = new QVBoxLayout; main->setContentsMargins(32,30,32,24); main->setSpacing(22); outer->addLayout(main,1);
        auto *top = new QHBoxLayout;
        auto *titles = new QVBoxLayout; titles->setSpacing(6);
        titles->addWidget(label(T("YOUR WINDOWS LIBRARY"), "eyebrow"));
        titles->addWidget(label(T("Apps"), "heading"));
        titles->addWidget(label(T("Everything you’ve installed. One place to manage it."), "muted"));
        top->addLayout(titles,1); refreshButton=button(T("↻  Refresh")); top->addWidget(refreshButton); main->addLayout(top);
        auto *stats = new QHBoxLayout; stats->setSpacing(14);
        auto makeCard=[&](const QString &title, QLabel *value){ auto *f=new QFrame; f->setObjectName("card"); auto *l=new QVBoxLayout(f); l->setContentsMargins(20,16,20,16); l->addWidget(label(title,"eyebrow")); l->addWidget(value); return f; };
        count=label("—","count"); engine=label(T("Loading …"),"engine"); engine->setWordWrap(true);
        stats->addWidget(makeCard(T("INSTALLED APPS"),count),1);
        stats->addWidget(makeCard(T("PROTON VERSION"),engine),2); main->addLayout(stats);
        search=new QLineEdit; search->setObjectName("search"); search->setPlaceholderText(T("Search for an app …")); search->setClearButtonEnabled(true); main->addWidget(search);
        auto *body=new QHBoxLayout; body->setSpacing(18);
        auto *left=new QVBoxLayout; list=new QListWidget; list->setObjectName("programList"); list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); left->addWidget(list);
        empty=label(T("Loading your apps …"),"muted"); empty->setAlignment(Qt::AlignCenter); empty->setWordWrap(true); left->addWidget(empty,1); body->addLayout(left,1);
        auto *detail=new QFrame; detail->setObjectName("detail"); detail->setFixedWidth(248);
        auto *dl=new QVBoxLayout(detail); dl->setContentsMargins(22,24,22,22); dl->setSpacing(18);
        badge=label("W","badge"); badge->setAlignment(Qt::AlignCenter); badge->setFixedSize(72,72); dl->addWidget(badge);
        detailName=label(T("Select an app"),"detailName"); detailName->setWordWrap(true); dl->addWidget(detailName);
        detailText=label(T("Your Windows apps, all in one place."),"muted"); detailText->setWordWrap(true); dl->addWidget(detailText);
        dl->addStretch(); removeButton=button(T("Uninstall app"), "danger"); removeButton->setEnabled(false); dl->addWidget(removeButton); body->addWidget(detail); main->addLayout(body,1);
        auto *actions=new QHBoxLayout; folderButton=button(T("Open Windows folder")); folderButton->setEnabled(false); configureButton=button(T("Settings")); actions->addWidget(folderButton); actions->addWidget(configureButton); actions->addStretch(); main->addLayout(actions);
        progress=new QProgressBar; progress->setRange(0,0); progress->setTextVisible(false); progress->hide(); main->addWidget(progress);
        status=label(T("Ready"), "muted"); status->setWordWrap(true); main->addWidget(status);
        process=new QProcess(this);
        connect(process,&QProcess::readyReadStandardOutput,this,[this]{output+=process->readAllStandardOutput();});
        connect(process,&QProcess::readyReadStandardError,this,[this]{process->readAllStandardError();});
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,&Manager::finished);
        connect(process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart){setBusy(false);status->setText(T("Could not start the WinBridge integration: ") + process->errorString());}});
        connect(search,&QLineEdit::textChanged,this,[this]{render();});
        connect(list,&QListWidget::currentItemChanged,this,[this]{updateSelection();});
        connect(refreshButton,&QPushButton::clicked,this,[this]{request("list");});
        connect(nav,&QPushButton::clicked,search,qOverload<>(&QWidget::setFocus));
        connect(folderButton,&QPushButton::clicked,this,[this]{QDesktopServices::openUrl(QUrl::fromLocalFile(prefix+"/pfx/drive_c"));});
        connect(configureButton,&QPushButton::clicked,this,[this]{showSettings();});
        connect(removeButton,&QPushButton::clicked,this,[this]{
            if(selectedKey.isEmpty()||busy)return;
            QMessageBox box(QMessageBox::Question,T("Uninstall app"), T("Uninstall “%1”?\n\nThe app’s own uninstall wizard will open.").arg(detailName->text()),QMessageBox::NoButton,this);
            auto *cancel=box.addButton(T("Cancel"),QMessageBox::RejectRole); auto *yes=box.addButton(T("Uninstall"),QMessageBox::AcceptRole);box.setDefaultButton(cancel);box.exec();
            if(box.clickedButton()==yes)request("uninstall",selectedKey);
        });
        if (!screenshot) QTimer::singleShot(0,this,[this]{request("list");});
        else {engine->setText(T("Not selected"));render();status->setText(T("Your environment is created when you first open an .exe file with WinBridge."));}
    }
};

#ifndef WINBRIDGE_MANAGER_TEST
int main(int argc, char **argv) {
    QApplication app(argc,argv);
    app.setWindowIcon(QIcon(":/assets/winbridge.png"));
    app.setApplicationName("WinBridge Manager"); app.setDesktopFileName("winbridge-manager");
    QCommandLineParser parser;parser.addHelpOption();parser.addOption({"backend","Path to the WinBridge integration module","path"});parser.addOption({"screenshot","Render an empty-state preview and exit","path"});parser.process(app);
    QString backend=parser.value("backend");
    if(backend.isEmpty()) {
        QDir bin(QCoreApplication::applicationDirPath());
        QStringList candidates{bin.filePath("manager_backend.py"),bin.filePath("../../manager_backend.py"),bin.filePath("../lib/winbridge/manager_backend.py")};
        for(const auto &candidate:candidates)if(QFileInfo::exists(candidate)){backend=QFileInfo(candidate).absoluteFilePath();break;}
    }
    int exitCode;
    do {
    I18n::load(QSettings("WinBridge", "Manager").value("language", "en-US").toString());
    Manager window(backend,parser.isSet("screenshot"));window.show();
    if(parser.isSet("screenshot"))QTimer::singleShot(500,&app,[&]{bool ok=window.grab().save(parser.value("screenshot"));app.exit(ok?0:1);});
    exitCode = app.exec();
    } while (exitCode == 42);
    return exitCode;
}

#endif
