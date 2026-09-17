/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "opener.h"
#include "executable_icon.h"
#include "launcher.h"
#include "shared_space.h"
#include "shortcuts.h"
#include "single_instance.h"

#include <QtWidgets>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProgressBar>
#include <QSaveFile>
#include <QScreen>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include <atomic>
#include <stdexcept>

#ifdef WINBRIDGE_HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#endif

namespace WinBridge {

QStringList parseArguments(const QString &argsText) {
    const QString trimmed = argsText.trimmed();
    if (trimmed.isEmpty()) return {};
    return QProcess::splitCommand(trimmed);
}

static QString normalizedExecutable(const QString &path) {
    QString expanded = path.trimmed();
    if (expanded.startsWith("~/")) expanded = QDir::homePath() + expanded.mid(1);
    QFileInfo info(expanded);
    const QString suffix = info.suffix().toLower();
    if (!info.isFile() || suffix != "exe") return {};
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical;
}

QString recentExecutablesPath() {
    QString config = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (config.isEmpty()) config = QDir::homePath() + "/.config";
    return QDir::cleanPath(config + "/winbridge/recent.json");
}

QStringList recentExecutables(const QString &customPath) {
    QFile file(customPath.isEmpty() ? recentExecutablesPath() : customPath);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) return {};
    QStringList result;
    for (const QJsonValue &value : document.array()) {
        const QString path = normalizedExecutable(value.toString());
        if (!path.isEmpty() && !result.contains(path)) result << path;
        if (result.size() == 12) break;
    }
    return result;
}

bool rememberExecutable(const QString &exePath, const QString &customPath) {
    const QString path = normalizedExecutable(exePath);
    if (path.isEmpty()) return false;
    QStringList recents = recentExecutables(customPath);
    recents.removeAll(path);
    recents.prepend(path);
    while (recents.size() > 12) recents.removeLast();

    const QString storagePath = customPath.isEmpty() ? recentExecutablesPath() : customPath;
    if (!QDir().mkpath(QFileInfo(storagePath).dir().absolutePath())) return false;
    QSaveFile file(storagePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QJsonArray array;
    for (const QString &recent : recents) array.append(recent);
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    return file.commit();
}

bool removeRecentExecutable(const QString &exePath, const QString &customPath) {
    const QString storagePath = customPath.isEmpty() ? recentExecutablesPath() : customPath;
    QFile readFile(storagePath);
    if (!readFile.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument document = QJsonDocument::fromJson(readFile.readAll());
    readFile.close();
    if (!document.isArray()) return false;

    const QString normalized = normalizedExecutable(exePath);
    const QString target = normalized.isEmpty() ? exePath.trimmed() : normalized;
    if (target.isEmpty()) return false;

    QJsonArray newArray;
    bool found = false;
    for (const QJsonValue &val : document.array()) {
        const QString entry = val.toString().trimmed();
        const QString normEntry = normalizedExecutable(entry);
        if (entry == target || (!normEntry.isEmpty() && normEntry == target)) {
            found = true;
        } else {
            newArray.append(entry);
        }
    }

    if (!found) return false;

    if (newArray.isEmpty()) {
        QFile::remove(storagePath);
        return true;
    }

    if (!QDir().mkpath(QFileInfo(storagePath).dir().absolutePath())) return false;
    QSaveFile file(storagePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(newArray).toJson(QJsonDocument::Indented));
    return file.commit();
}

QString managerExecutablePath(const QString &launcherPath) {
    QStringList candidates;
    if (!launcherPath.isEmpty()) {
        candidates << QFileInfo(launcherPath).absoluteDir().filePath("winbridge-manager");
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << QDir(appDir).filePath("winbridge-manager")
               << QDir(appDir).filePath("manager/winbridge-manager");
    const QString fromPath = QStandardPaths::findExecutable("winbridge-manager");
    if (!fromPath.isEmpty()) candidates << fromPath;
    candidates << QDir::homePath() + "/.local/bin/winbridge-manager"
               << QDir::homePath() + "/.local/share/winbridge/winbridge-manager"
               << "/usr/local/bin/winbridge-manager"
               << "/usr/bin/winbridge-manager";
    for (const QString &candidate : candidates) {
        QFileInfo info(candidate);
        if (info.isFile() && info.isExecutable()) {
            const QString canonical = info.canonicalFilePath();
            return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
        }
    }
    return {};
}

static QIcon resolveIcon() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + "/../share/pixmaps/winbridge.png",
        appDir + "/../assets/winbridge-app-launcher.png",
        appDir + "/../../assets/winbridge-app-launcher.png",
        "/usr/local/share/pixmaps/winbridge.png",
        "/usr/share/pixmaps/winbridge.png"
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo(candidate).isFile()) return QIcon(candidate);
    }
    return QIcon::fromTheme("winbridge");
}

static QIcon resolveManagerIcon() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + "/../share/pixmaps/winbridge-manager.png",
        appDir + "/../assets/winbridge-manager-icon.png",
        appDir + "/../../assets/winbridge-manager-icon.png",
        "/usr/local/share/pixmaps/winbridge-manager.png",
        "/usr/share/pixmaps/winbridge-manager.png"
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo(candidate).isFile()) return QIcon(candidate);
    }
    return QIcon::fromTheme("winbridge-manager");
}

static QString styleSheetText(const QString &requestedTheme) {
    const QString theme = requestedTheme.isEmpty()
        ? QSettings("WinBridge", "Manager").value("theme", "classic").toString()
        : requestedTheme;
    const bool dark = theme == "dark";
    QString style = R"(
        QWidget { background: {{face}}; color: {{text}}; font-family: 'Segoe UI','Tahoma','MS Sans Serif','DejaVu Sans',sans-serif; font-size: 12px; }
        QFrame#topNav { background: {{nav}}; border-bottom: 2px solid {{shadow}}; }
        QLabel { background: transparent; }
        QLabel#logo { background: transparent; border: none; color: white; font-size: 28px; font-weight: bold; }
        QLabel#brand { color: white; font-size: 29px; font-weight: bold; }
        QLabel#brandSub { color: #ffeb80; font-size: 12px; font-weight: bold; letter-spacing: 1.5px; }
        QLabel#heading { color: {{accent}}; font-size: 22px; font-weight: bold; }
        QLabel#muted, QLabel#status { color: {{muted}}; }
        QLabel#sectionLabel { color: {{text}}; font-size: 12px; font-weight: bold; }
        QLabel#loadingLabel { color: {{text}}; font-size: 13px; font-weight: bold; }
        QCheckBox { color: {{text}}; spacing: 7px; font-size: 12px; font-weight: bold; background: transparent; }
        QCheckBox::indicator { width: 14px; height: 14px; border-top: 2px solid {{shadow}}; border-left: 2px solid {{shadow}}; border-right: 2px solid {{light}}; border-bottom: 2px solid {{light}}; background: {{window}}; border-radius: 0px; }
        QCheckBox::indicator:hover { background: {{hover}}; }
        QCheckBox::indicator:checked { background: {{accent}}; border-top: 2px solid {{darkshadow}}; border-left: 2px solid {{darkshadow}}; border-right: 2px solid {{light}}; border-bottom: 2px solid {{light}}; }
        QLineEdit { min-height: 34px; padding: 0 8px; background: {{window}}; color: {{text}}; border-top: 2px solid {{shadow}}; border-left: 2px solid {{shadow}}; border-right: 2px solid {{light}}; border-bottom: 2px solid {{light}}; selection-background-color: {{accent}}; }
        QListWidget { background: {{window}}; color: {{text}}; border-top: 2px solid {{shadow}}; border-left: 2px solid {{shadow}}; border-right: 2px solid {{light}}; border-bottom: 2px solid {{light}}; outline: none; padding: 3px; }
        QListWidget::item { padding: 7px 9px; border-bottom: 1px solid {{divider}}; }
        QListWidget::item:selected { background: {{selected}}; color: {{text}}; border: 1px dotted {{accent}}; }
        QListWidget::item:hover:!selected { background: {{hover}}; }
        QLabel#emptyLabel { color: {{muted}}; background: {{sunken}}; border-top: 2px solid {{shadow}}; border-left: 2px solid {{shadow}}; border-right: 2px solid {{light}}; border-bottom: 2px solid {{light}}; padding: 28px; }
        QPushButton { min-height: 32px; padding: 4px 13px; background: {{face}}; color: {{text}}; border-top: 2px solid {{light}}; border-left: 2px solid {{light}}; border-right: 2px solid {{darkshadow}}; border-bottom: 2px solid {{darkshadow}}; border-radius: 0; font-weight: bold; }
        QPushButton:hover { background: {{hover}}; }
        QPushButton:pressed { background: {{pressed}}; border-top-color: {{darkshadow}}; border-left-color: {{darkshadow}}; border-right-color: {{light}}; border-bottom-color: {{light}}; }
        QPushButton:disabled { color: {{shadow}}; border-right-color: {{shadow}}; border-bottom-color: {{shadow}}; }
        QPushButton#primary { min-width: 92px; }
        QPushButton#shortcutButton { min-width: 160px; }
        QPushButton#managerButton { min-height: 34px; padding: 3px 12px; }
        QToolButton#clearButton { border: none; color: {{accent}}; padding: 4px; background: transparent; font-weight: bold; }
        QMenu { background: {{face}}; color: {{text}}; border-top: 2px solid {{light}}; border-left: 2px solid {{light}}; border-right: 2px solid {{darkshadow}}; border-bottom: 2px solid {{darkshadow}}; padding: 3px; }
        QMenu::item { padding: 7px 28px 7px 10px; }
        QMenu::item:selected { background: {{selected}}; }
        QMenu::separator { height: 1px; background: {{divider}}; margin: 4px 2px; }
        QProgressBar#loadingBar {
            border-top: 2px solid {{shadow}};
            border-left: 2px solid {{shadow}};
            border-right: 2px solid {{light}};
            border-bottom: 2px solid {{light}};
            background: {{window}};
            text-align: center;
            height: 20px;
        }
        QProgressBar#loadingBar::chunk {
            background: {{accent}};
        }
    )";
    const QMap<QString, QString> colors = dark ? QMap<QString, QString>{
        {"face", "#2c2f35"}, {"text", "#eef0f3"}, {"muted", "#9aa0aa"}, {"accent", "#7ea8f8"},
        {"window", "#17181c"}, {"sunken", "#222429"}, {"light", "#525763"}, {"shadow", "#181a1d"},
        {"darkshadow", "#0e0f11"}, {"hover", "#373b43"}, {"pressed", "#202226"}, {"selected", "#193563"},
        {"divider", "#26282e"}, {"nav", "qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #101624,stop:.75 #182236,stop:1 #243450)"}
    } : QMap<QString, QString>{
        {"face", "#c0c0c0"}, {"text", "#000000"}, {"muted", "#444444"}, {"accent", "#000080"},
        {"window", "#ffffff"}, {"sunken", "#ece9d8"}, {"light", "#ffffff"}, {"shadow", "#808080"},
        {"darkshadow", "#404040"}, {"hover", "#d4d4d4"}, {"pressed", "#b8b8b8"}, {"selected", "#c8daf8"},
        {"divider", "#d4d0c8"}, {"nav", "qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #000080,stop:.75 #083b82,stop:1 #1084d0)"}
    };
    for (auto it = colors.cbegin(); it != colors.cend(); ++it) style.replace("{{" + it.key() + "}}", it.value());
    return style;
}

class ExecutableOpener final : public QDialog {
public:
    explicit ExecutableOpener(const QString &launcher, const QString &theme,
                             QWidget *parent = nullptr, const QString &recentStoragePath = "")
        : QDialog(parent), launcherPath(launcher), recentStorage(recentStoragePath) {
        setObjectName("executableOpener");
        setWindowTitle("WinBridge");
        setWindowIcon(resolveIcon());
        setMinimumSize(720, 600);
        resize(780, 680);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        auto *topNav = new QFrame;
        topNav->setObjectName("topNav");
        auto *headingRow = new QHBoxLayout(topNav);
        headingRow->setContentsMargins(24, 12, 24, 12);
        headingRow->setSpacing(14);
        auto *logo = new QLabel;
        logo->setObjectName("logo");
        logo->setFixedSize(64, 64);
        logo->setAlignment(Qt::AlignCenter);
        QPixmap iconPixmap = windowIcon().pixmap(52, 52);
        if (!iconPixmap.isNull()) logo->setPixmap(iconPixmap);
        else logo->setText("W");
        headingRow->addWidget(logo);
        auto *titles = new QVBoxLayout;
        titles->setSpacing(3);
        auto *title = new QLabel("WinBridge");
        title->setObjectName("brand");
        auto *subtitle = new QLabel(tr("EXE APP LAUNCHER"));
        subtitle->setObjectName("brandSub");
        subtitle->setWordWrap(true);
        titles->addWidget(title);
        titles->addWidget(subtitle);
        headingRow->addLayout(titles, 1);
        auto *managerButton = new QPushButton(resolveManagerIcon(), tr("WinBridge Manager"));
        managerButton->setObjectName("managerButton");
        managerButton->setIconSize(QSize(22, 22));
        managerButton->setCursor(Qt::PointingHandCursor);
        managerButton->setToolTip(tr("Open WinBridge Manager"));
        headingRow->addWidget(managerButton, 0, Qt::AlignVCenter);
        root->addWidget(topNav);

        auto *content = new QVBoxLayout;
        content->setContentsMargins(28, 22, 28, 24);
        content->setSpacing(16);
        root->addLayout(content, 1);

        auto *intro = new QLabel(tr("Open a Windows app"));
        intro->setObjectName("heading");
        content->addWidget(intro);
        auto *introText = new QLabel(tr("Choose an .exe file to run with your shared WinBridge environment."));
        introText->setObjectName("muted");
        introText->setWordWrap(true);
        content->addWidget(introText);

        auto *pathLabel = new QLabel(tr("Executable path"));
        pathLabel->setObjectName("sectionLabel");
        content->addWidget(pathLabel);
        auto *pathRow = new QHBoxLayout;
        pathRow->setSpacing(10);
        pathEdit = new QLineEdit;
        pathEdit->setObjectName("pathEdit");
        pathEdit->setPlaceholderText(tr("/path/to/application.exe"));
        pathEdit->setClearButtonEnabled(true);
        auto *browse = new QPushButton(tr("Browse…"));
        browse->setObjectName("browseButton");
        browse->setCursor(Qt::PointingHandCursor);
        pathRow->addWidget(pathEdit, 1);
        pathRow->addWidget(browse);
        content->addLayout(pathRow);

        argsCheck = new QCheckBox(tr("Optional arguments"));
        argsCheck->setObjectName("argsCheck");
        argsCheck->setCursor(Qt::PointingHandCursor);
        content->addWidget(argsCheck);

        argsEdit = new QLineEdit;
        argsEdit->setObjectName("argsEdit");
        argsEdit->setPlaceholderText(tr("-arg --flag \"value with spaces\" /switch"));
        argsEdit->setClearButtonEnabled(true);
        argsEdit->setVisible(false);
        content->addWidget(argsEdit);

        auto *recentHeader = new QHBoxLayout;
        auto *recentTitle = new QLabel(tr("Recently opened apps"));
        recentTitle->setObjectName("sectionLabel");
        recentHeader->addWidget(recentTitle);
        recentHeader->addStretch();
        clearButton = new QToolButton;
        clearButton->setText(tr("Clear list"));
        clearButton->setObjectName("clearButton");
        clearButton->setCursor(Qt::PointingHandCursor);
        recentHeader->addWidget(clearButton);
        content->addLayout(recentHeader);

        list = new QListWidget;
        list->setObjectName("recentList");
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setAlternatingRowColors(false);
        list->setIconSize(QSize(40, 40));
        list->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(list, &QListWidget::customContextMenuRequested, this, &ExecutableOpener::showRecentContextMenu);
        content->addWidget(list, 1);

        emptyLabel = new QLabel(tr("Apps you open with WinBridge will appear here."));
        emptyLabel->setObjectName("emptyLabel");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setWordWrap(true);
        content->addWidget(emptyLabel, 1);

        auto *statusRow = new QHBoxLayout;
        status = new QLabel(tr("Select an app or enter its path."));
        status->setObjectName("status");
        status->setWordWrap(true);
        statusRow->addWidget(status, 1);
        content->addLayout(statusRow);

        auto *buttons = new QHBoxLayout;
        buttons->setSpacing(10);
        shortcutButton = new QPushButton(tr("Make Shortcut"));
        shortcutButton->setObjectName("shortcutButton");
        shortcutButton->setCursor(Qt::PointingHandCursor);
        auto *shortcutMenu = new QMenu(shortcutButton);
        auto *desktopAction = shortcutMenu->addAction(tr("Desktop"));
        auto *menuAction = shortcutMenu->addAction(tr("Start Menu"));
        shortcutButton->setMenu(shortcutMenu);
        buttons->addWidget(shortcutButton);
        buttons->addStretch();
        auto *cancel = new QPushButton(tr("Cancel"));
        cancel->setObjectName("cancelButton");
        cancel->setCursor(Qt::PointingHandCursor);
        openButton = new QPushButton(tr("Open"));
        openButton->setObjectName("primary");
        openButton->setDefault(true);
        openButton->setCursor(Qt::PointingHandCursor);
        buttons->addWidget(cancel);
        buttons->addWidget(openButton);
        content->addLayout(buttons);

        connect(browse, &QPushButton::clicked, this, [this] {
            const QString current = normalizedExecutable(pathEdit->text());
            const QString start = current.isEmpty() ? QDir::homePath() : QFileInfo(current).dir().absolutePath();
            const QString selected = QFileDialog::getOpenFileName(this, tr("Choose a Windows app"), start,
                tr("Windows apps (*.exe *.EXE);;All files (*)"));
            if (!selected.isEmpty()) pathEdit->setText(selected);
        });
        connect(pathEdit, &QLineEdit::textChanged, this, [this] { updateSelection(); });
        connect(pathEdit, &QLineEdit::returnPressed, this, [this] { openSelected(); });
        connect(argsCheck, &QCheckBox::toggled, this, [this](bool checked) {
            argsEdit->setVisible(checked);
            if (checked) argsEdit->setFocus();
            updateSelection();
        });
        connect(argsEdit, &QLineEdit::textChanged, this, [this] { updateSelection(); });
        connect(argsEdit, &QLineEdit::returnPressed, this, [this] { openSelected(); });
        connect(list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
            if (item) pathEdit->setText(item->data(Qt::UserRole).toString());
            updateSelection();
        });
        connect(list, &QListWidget::itemDoubleClicked, this, [this] { openSelected(); });
        connect(clearButton, &QToolButton::clicked, this, [this] {
            QFile::remove(recentPath());
            populateRecents();
            pathEdit->clear();
            argsEdit->clear();
            argsCheck->setChecked(false);
        });
        connect(desktopAction, &QAction::triggered, this, [this] { makeShortcut(ShortcutLocation::Desktop); });
        connect(menuAction, &QAction::triggered, this, [this] { makeShortcut(ShortcutLocation::StartMenu); });
        connect(managerButton, &QPushButton::clicked, this, [this] {
            const QString token = activationTokenForWindow(windowHandle(), "winbridge-manager");
            if (activateRunningInstance("manager", token)) return;
            const QString manager = managerExecutablePath(launcherPath);
            if (manager.isEmpty()) {
                QMessageBox::critical(this, tr("WinBridge"),
                    tr("WinBridge Manager could not be found. Install it and try again."));
                return;
            }
            if (!QProcess::startDetached(manager, {})) {
                QMessageBox::critical(this, tr("WinBridge"),
                    tr("WinBridge Manager could not be opened."));
            }
        });
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(openButton, &QPushButton::clicked, this, [this] { openSelected(); });

        setTabOrder(pathEdit, browse);
        setTabOrder(browse, argsCheck);
        setTabOrder(argsCheck, argsEdit);
        setTabOrder(argsEdit, list);
        setTabOrder(list, shortcutButton);
        setTabOrder(shortcutButton, cancel);
        setTabOrder(cancel, openButton);

        setStyleSheet(styleSheetText(theme));
        populateRecents();
        updateSelection();
        if (list->count() > 0) list->setFocus();
        else pathEdit->setFocus();
    }

    QString executable() const { return selectedExecutable; }
    QStringList arguments() const { return selectedArguments; }
    QString argumentsText() const { return selectedArgumentsText; }

private:
    QString launcherPath;
    QString recentStorage;
    QString selectedExecutable;
    QStringList selectedArguments;
    QString selectedArgumentsText;
    QLineEdit *pathEdit = nullptr;
    QCheckBox *argsCheck = nullptr;
    QLineEdit *argsEdit = nullptr;
    QListWidget *list = nullptr;
    QLabel *emptyLabel = nullptr;
    QLabel *status = nullptr;
    QPushButton *openButton = nullptr;
    QPushButton *shortcutButton = nullptr;
    QToolButton *clearButton = nullptr;

    QString recentPath() const {
        return recentStorage.isEmpty() ? recentExecutablesPath() : recentStorage;
    }

    void populateRecents() {
        list->clear();
        for (const QString &path : recentExecutables(recentPath())) {
            QFileInfo info(path);
            auto *item = new QListWidgetItem;
            item->setText(info.completeBaseName() + "\n" + info.dir().absolutePath());
            item->setToolTip(path);
            item->setData(Qt::UserRole, path);
            item->setData(Qt::UserRole + 1, info.dir().absolutePath());
            const QString iconPath = executableIconPath(path);
            item->setIcon(iconPath.isEmpty()
                ? QIcon::fromTheme("application-x-executable")
                : QIcon(iconPath));
            item->setSizeHint(QSize(0, 62));
            list->addItem(item);
        }
        const bool hasItems = list->count() > 0;
        list->setVisible(hasItems);
        emptyLabel->setVisible(!hasItems);
        clearButton->setVisible(hasItems);
        if (hasItems) list->setCurrentRow(0);
    }

    void updateSelection() {
        const QString valid = normalizedExecutable(pathEdit->text());
        openButton->setEnabled(!valid.isEmpty());
        shortcutButton->setEnabled(!valid.isEmpty());
        if (valid.isEmpty()) {
            status->setText(tr("Select an app or enter its path."));
        } else {
            const bool hasArgs = argsCheck && argsCheck->isChecked() && argsEdit && !argsEdit->text().trimmed().isEmpty();
            const QString args = hasArgs ? argsEdit->text().trimmed() : QString();
            status->setText(args.isEmpty() ? valid : QString("%1 %2").arg(valid, args));
        }
    }

    void openSelected() {
        const QString valid = normalizedExecutable(pathEdit->text());
        if (valid.isEmpty()) {
            status->setText(tr("Choose an existing .exe file."));
            return;
        }
        selectedExecutable = valid;
        const bool useArgs = argsCheck && argsCheck->isChecked();
        selectedArguments = useArgs ? parseArguments(argsEdit ? argsEdit->text() : QString()) : QStringList();
        selectedArgumentsText = useArgs && argsEdit ? argsEdit->text().trimmed() : QString();
        accept();
    }

    void makeShortcut(ShortcutLocation location, const QString &exePath = QString()) {
        const QString target = exePath.isEmpty() ? pathEdit->text() : exePath;
        const QString valid = normalizedExecutable(target);
        if (valid.isEmpty()) return;
        try {
            const bool useArgs = argsCheck && argsCheck->isChecked();
            const QStringList args = useArgs ? parseArguments(argsEdit ? argsEdit->text() : QString()) : QStringList();
            const QString created = createExecutableShortcut(
                valid, launcherPath, location, "", "", args);
            status->setText(location == ShortcutLocation::Desktop
                ? tr("Desktop shortcut created: %1").arg(created)
                : tr("Start Menu shortcut created: %1").arg(created));
        } catch (const std::exception &error) {
            QMessageBox::critical(this, tr("WinBridge"), QString::fromUtf8(error.what()));
        }
    }

    void showFiles(const QString &filePath) {
        const QFileInfo info(filePath);
        const QString dir = info.dir().absolutePath();
        if (dir.isEmpty() || !QDir(dir).exists()) {
            status->setText(tr("Folder is unavailable."));
        } else if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dir))) {
            status->setText(tr("Could not open the folder."));
        } else {
            status->setText(tr("Opened folder: %1").arg(dir));
        }
    }

    void removeRecent(const QString &path) {
        removeRecentExecutable(path, recentPath());
        populateRecents();
        if (normalizedExecutable(pathEdit->text()) == normalizedExecutable(path)) {
            if (list->count() > 0) {
                list->setCurrentRow(0);
            } else {
                pathEdit->clear();
                argsEdit->clear();
                argsCheck->setChecked(false);
            }
        }
        updateSelection();
    }

    void showRecentContextMenu(const QPoint &pos) {
        auto *item = list->itemAt(pos);
        if (!item && pos == QPoint(-1, -1)) {
            item = list->currentItem();
        }
        if (!item) return;
        list->setCurrentItem(item);

        const QString path = item->data(Qt::UserRole).toString();
        if (path.isEmpty()) return;

        auto *menu = new QMenu(list);
        menu->setObjectName("recentContextMenu");
        menu->setAttribute(Qt::WA_DeleteOnClose);

        auto *shortcutSubMenu = menu->addMenu(tr("Make Shortcut"));
        shortcutSubMenu->setObjectName("recentMakeShortcutMenu");
        auto *desktopAction = shortcutSubMenu->addAction(tr("Desktop"));
        desktopAction->setObjectName("recentDesktopAction");
        auto *menuAction = shortcutSubMenu->addAction(tr("Start Menu"));
        menuAction->setObjectName("recentStartMenuAction");

        connect(desktopAction, &QAction::triggered, this, [this, path] {
            makeShortcut(ShortcutLocation::Desktop, path);
        });
        connect(menuAction, &QAction::triggered, this, [this, path] {
            makeShortcut(ShortcutLocation::StartMenu, path);
        });

        auto *showFilesAction = menu->addAction(tr("Show Files"));
        showFilesAction->setObjectName("recentShowFilesAction");
        connect(showFilesAction, &QAction::triggered, this, [this, path] {
            showFiles(path);
        });

        menu->addSeparator();

        auto *removeAction = menu->addAction(tr("Remove From List"));
        removeAction->setObjectName("recentRemoveAction");
        connect(removeAction, &QAction::triggered, this, [this, path] {
            removeRecent(path);
        });

        const QPoint globalPos = (pos == QPoint(-1, -1))
            ? list->viewport()->mapToGlobal(list->visualItemRect(item).center())
            : list->viewport()->mapToGlobal(pos);
        menu->popup(globalPos);
    }

};

OpenRequest showExecutableOpener(
    const QString &launcher,
    QWidget *parent,
    const QString &screenshotPath,
    const QString &theme,
    const QString &recentStoragePath
) {
    ExecutableOpener dialog(launcher, theme, parent, recentStoragePath);
    InstanceActivationServer activationServer("launcher", [&dialog](const QString &token) {
        bringWindowToForeground(&dialog, token);
    });
    if (screenshotPath.isEmpty()) activationServer.start();
    OpenRequest request;
    if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(250, &dialog, [&] {
            request.screenshotSaved = dialog.grab().save(screenshotPath);
            dialog.reject();
        });
    }
    request.accepted = dialog.exec() == QDialog::Accepted;
    request.executable = dialog.executable();
    request.arguments = dialog.arguments();
    request.argumentsText = dialog.argumentsText();
    return request;
}

AppLoadingDialog::AppLoadingDialog(const QString &exePath, const QString &theme, QWidget *parent)
    : QDialog(parent) {
    setObjectName("appLoadingDialog");
    setWindowTitle("WinBridge");
    setWindowIcon(resolveIcon());
    setModal(true);
    setFixedSize(500, 165);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *topNav = new QFrame;
    topNav->setObjectName("topNav");
    auto *headingRow = new QHBoxLayout(topNav);
    headingRow->setContentsMargins(20, 10, 20, 10);
    headingRow->setSpacing(12);

    auto *logo = new QLabel;
    logo->setObjectName("logo");
    logo->setFixedSize(48, 48);
    logo->setAlignment(Qt::AlignCenter);
    const QPixmap iconPixmap = windowIcon().pixmap(40, 40);
    if (!iconPixmap.isNull()) logo->setPixmap(iconPixmap);
    else logo->setText("W");
    headingRow->addWidget(logo);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto *title = new QLabel("WinBridge");
    title->setObjectName("brand");
    auto *subtitle = new QLabel(tr("EXE APP LAUNCHER"));
    subtitle->setObjectName("brandSub");
    subtitle->setWordWrap(true);
    titles->addWidget(title);
    titles->addWidget(subtitle);
    headingRow->addLayout(titles, 1);

    root->addWidget(topNav);

    auto *content = new QVBoxLayout;
    content->setContentsMargins(24, 16, 24, 18);
    content->setSpacing(12);

    QString displayName = QFileInfo(exePath).fileName();
    if (displayName.isEmpty()) displayName = exePath;

    auto *loadingLabel = new QLabel(tr("Loading windows app %1").arg(displayName));
    loadingLabel->setObjectName("loadingLabel");
    loadingLabel->setWordWrap(true);
    content->addWidget(loadingLabel);

    auto *bar = new QProgressBar;
    bar->setObjectName("loadingBar");
    bar->setRange(0, 0);
    bar->setTextVisible(false);
    content->addWidget(bar);

    root->addLayout(content, 1);

    setStyleSheet(styleSheetText(theme));

    if (auto *screen = QGuiApplication::primaryScreen()) {
        const QRect geom = screen->availableGeometry();
        move(geom.center() - QPoint(width() / 2, height() / 2));
    }
}

QSet<qint64> matchingProcessPids(
    const QString &exePath,
    const QString &prefix,
    const QString &procRoot
) {
    Q_UNUSED(prefix);
    QString targetExe = exePath;
    if (QFileInfo(targetExe).suffix().toLower() == "lnk") {
        const QString resolved = shortcutTarget(targetExe, prefix);
        if (!resolved.isEmpty()) {
            targetExe = resolved;
        }
    }
    const QString exeName = QFileInfo(targetExe).fileName().toLower();
    if (exeName.isEmpty()) return {};

    const qint64 myPid = QCoreApplication::applicationPid();
    QSet<qint64> pids;
    QDir procDir(procRoot.isEmpty() ? "/proc" : procRoot);
    if (!procDir.exists()) return {};

    const QFileInfoList entries = procDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        bool ok = false;
        const qint64 pid = entry.fileName().toLongLong(&ok);
        if (!ok || pid <= 0 || pid == myPid) continue;

        QFile cmdlineFile(entry.filePath() + "/cmdline");
        if (!cmdlineFile.open(QIODevice::ReadOnly)) continue;
        const QByteArray raw = cmdlineFile.readAll();
        cmdlineFile.close();
        if (raw.isEmpty()) continue;

        const QByteArrayList rawTokens = raw.split('\0');
        QStringList tokens;
        for (const QByteArray &tok : rawTokens) {
            if (!tok.isEmpty()) tokens.append(QString::fromLocal8Bit(tok));
        }
        if (tokens.isEmpty()) continue;

        const QString progName = QFileInfo(tokens.first().trimmed().remove('"')).fileName().toLower();
        if (progName == "winbridge" || progName == "python" || progName == "python3" ||
            progName == "bash" || progName == "sh" || progName == "umu-run") {
            continue;
        }

        for (const QString &tok : tokens) {
            QString clean = tok.trimmed();
            if (clean.startsWith('"') && clean.endsWith('"') && clean.size() >= 2) {
                clean = clean.mid(1, clean.size() - 2);
            }
            clean.replace('\\', '/');
            const QString fn = QFileInfo(clean).fileName().toLower();
            if (fn == exeName) {
                pids.insert(pid);
                break;
            }
        }
    }

    return pids;
}

bool isExecutableStarted(
    const QString &exePath,
    const QString &prefix,
    const QSet<qint64> &initialPids,
    const QString &procRoot
) {
    const QSet<qint64> currentPids = matchingProcessPids(exePath, prefix, procRoot);
    for (qint64 pid : currentPids) {
        if (!initialPids.contains(pid)) {
            return true;
        }
    }
    return false;
}

QSet<unsigned long> activeWindowIds() {
    QSet<unsigned long> result;
#ifdef WINBRIDGE_HAVE_X11
    Display *display = XOpenDisplay(nullptr);
    if (!display) return result;

    Atom netClientList = XInternAtom(display, "_NET_CLIENT_LIST", True);
    if (netClientList != None) {
        Atom actualType;
        int actualFormat;
        unsigned long numItems = 0, bytesAfter = 0;
        unsigned char *prop = nullptr;
        if (XGetWindowProperty(display, DefaultRootWindow(display), netClientList, 0, 4096, False, XA_WINDOW,
                               &actualType, &actualFormat, &numItems, &bytesAfter, &prop) == Success && prop) {
            Window *windows = reinterpret_cast<Window*>(prop);
            for (unsigned long i = 0; i < numItems; ++i) {
                result.insert(static_cast<unsigned long>(windows[i]));
            }
            XFree(prop);
        }
    }
    XCloseDisplay(display);
#endif
    return result;
}

bool isExecutableWindowVisible(
    const QString &exePath,
    const QString &prefix,
    const QSet<qint64> &initialPids,
    const QSet<unsigned long> &initialWindows,
    const QString &procRoot
) {
    const QSet<qint64> currentPids = matchingProcessPids(exePath, prefix, procRoot);
    if (currentPids.isEmpty()) {
        return false;
    }

    QSet<qint64> appPids;
    for (qint64 pid : currentPids) {
        if (!initialPids.contains(pid)) {
            appPids.insert(pid);
        }
    }
    if (appPids.isEmpty()) {
        appPids = currentPids;
    }

#ifdef WINBRIDGE_HAVE_X11
    Display *display = XOpenDisplay(nullptr);
    if (display) {
        Atom netClientList = XInternAtom(display, "_NET_CLIENT_LIST", True);
        Atom netPid = XInternAtom(display, "_NET_WM_PID", True);
        Atom netName = XInternAtom(display, "_NET_WM_NAME", True);
        Atom utf8String = XInternAtom(display, "UTF8_STRING", True);

        QString targetExe = exePath;
        if (QFileInfo(targetExe).suffix().toLower() == "lnk") {
            const QString resolved = shortcutTarget(targetExe, prefix);
            if (!resolved.isEmpty()) {
                targetExe = resolved;
            }
        }
        const QString exeName = QFileInfo(targetExe).fileName().toLower();
        const QString exeBaseName = QFileInfo(targetExe).completeBaseName().toLower();
        const qint64 myPid = QCoreApplication::applicationPid();

        Atom actualType;
        int actualFormat;
        unsigned long numItems = 0, bytesAfter = 0;
        unsigned char *prop = nullptr;
        bool found = false;

        if (netClientList != None &&
            XGetWindowProperty(display, DefaultRootWindow(display), netClientList, 0, 4096, False, XA_WINDOW,
                               &actualType, &actualFormat, &numItems, &bytesAfter, &prop) == Success && prop) {
            Window *windows = reinterpret_cast<Window*>(prop);
            for (unsigned long i = 0; i < numItems; ++i) {
                Window w = windows[i];
                if (initialWindows.contains(static_cast<unsigned long>(w))) {
                    continue;
                }

                XWindowAttributes attr;
                if (!XGetWindowAttributes(display, w, &attr) || attr.map_state != IsViewable ||
                    attr.width < 10 || attr.height < 10) {
                    continue;
                }

                unsigned long winPid = 0;
                unsigned char *pidProp = nullptr;
                if (netPid != None &&
                    XGetWindowProperty(display, w, netPid, 0, 1, False, XA_CARDINAL,
                                       &actualType, &actualFormat, &numItems, &bytesAfter, &pidProp) == Success && pidProp) {
                    winPid = *reinterpret_cast<unsigned long*>(pidProp);
                    XFree(pidProp);
                }

                if (winPid != 0 && winPid == static_cast<unsigned long>(myPid)) {
                    continue;
                }

                if (winPid != 0 && appPids.contains(static_cast<qint64>(winPid))) {
                    found = true;
                    break;
                }

                XClassHint hint;
                if (XGetClassHint(display, w, &hint)) {
                    QString resName = hint.res_name ? QString::fromUtf8(hint.res_name).toLower() : QString();
                    QString resClass = hint.res_class ? QString::fromUtf8(hint.res_class).toLower() : QString();
                    if (hint.res_name) XFree(hint.res_name);
                    if (hint.res_class) XFree(hint.res_class);

                    if (resName == "winbridge" || resClass == "winbridge") {
                        continue;
                    }

                    if (resName == exeName || resClass == exeName ||
                        resName == exeBaseName || resClass == exeBaseName ||
                        resClass == "wine" || resName == "wine") {
                        found = true;
                        break;
                    }
                }

                QString winTitle;
                unsigned char *nameProp = nullptr;
                if (netName != None && utf8String != None &&
                    XGetWindowProperty(display, w, netName, 0, 256, False, utf8String,
                                       &actualType, &actualFormat, &numItems, &bytesAfter, &nameProp) == Success && nameProp) {
                    winTitle = QString::fromUtf8(reinterpret_cast<char*>(nameProp));
                    XFree(nameProp);
                } else {
                    char *fetchName = nullptr;
                    if (XFetchName(display, w, &fetchName) && fetchName) {
                        winTitle = QString::fromUtf8(fetchName);
                        XFree(fetchName);
                    }
                }

                if (!winTitle.isEmpty()) {
                    if (winTitle.contains("WinBridge", Qt::CaseInsensitive)) {
                        continue;
                    }
                    if (winTitle.toLower().contains(exeBaseName)) {
                        found = true;
                        break;
                    }
                }
            }
            XFree(prop);
        }
        XCloseDisplay(display);
        if (found) {
            return true;
        }
        return false;
    }
#endif

    return !appPids.isEmpty();
}

int launchWithLoadingDialog(
    const QString &exe,
    const QString &proton,
    const QStringList &roots,
    const QStringList &libs,
    const QStringList &extra,
    const QString &prefix,
    const QString &theme,
    const QString &launcher
) {
    const bool hasDisplay = !qEnvironmentVariable("DISPLAY").isEmpty() || !qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty();
    const bool isGuiApp = qobject_cast<QApplication*>(QCoreApplication::instance()) != nullptr;

    if (!hasDisplay || !isGuiApp) {
        return launch(exe, proton, roots, libs, extra, prefix, false, "run", nullptr, launcher);
    }

    const QSet<qint64> initialPids = matchingProcessPids(exe, prefix);
    const QSet<unsigned long> initialWindows = activeWindowIds();

    std::atomic<bool> launchDone{false};
    std::atomic<int> launchExitCode{0};
    std::exception_ptr launchException = nullptr;

    QThread *workerThread = QThread::create([&] {
        try {
            launchExitCode.store(launch(exe, proton, roots, libs, extra, prefix, false, "run", nullptr, launcher));
        } catch (...) {
            launchException = std::current_exception();
        }
        launchDone.store(true);
    });

    AppLoadingDialog dialog(exe, theme);

    QTimer pollTimer;
    pollTimer.setInterval(100);

    int elapsedMs = 0;
    int processStartedMs = 0;
    const int maxWindowWaitMs = 15000;
    const int safetyTimeoutMs = 35000;

    QObject::connect(&pollTimer, &QTimer::timeout, &dialog, [&] {
        elapsedMs += 100;
        if (launchDone.load()) {
            pollTimer.stop();
            dialog.accept();
            return;
        }
        if (isExecutableWindowVisible(exe, prefix, initialPids, initialWindows)) {
            pollTimer.stop();
            dialog.accept();
            return;
        }
        if (isExecutableStarted(exe, prefix, initialPids)) {
            processStartedMs += 100;
            if (processStartedMs >= maxWindowWaitMs) {
                pollTimer.stop();
                dialog.accept();
                return;
            }
        }
        if (elapsedMs >= safetyTimeoutMs) {
            pollTimer.stop();
            dialog.accept();
            return;
        }
    });

    workerThread->start();
    pollTimer.start();
    dialog.exec();
    pollTimer.stop();

    while (!launchDone.load()) {
        if (workerThread->wait(150)) {
            break;
        }
        QCoreApplication::processEvents();
    }
    workerThread->wait();
    delete workerThread;

    if (launchException) {
        std::rethrow_exception(launchException);
    }

    return launchExitCode.load();
}

} // namespace WinBridge
