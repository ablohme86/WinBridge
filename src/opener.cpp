/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "opener.h"
#include "shortcuts.h"

#include <QtWidgets>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <stdexcept>

namespace WinBridge {

static QString normalizedExecutable(const QString &path) {
    QString expanded = path.trimmed();
    if (expanded.startsWith("~/")) expanded = QDir::homePath() + expanded.mid(1);
    QFileInfo info(expanded);
    const QString suffix = info.suffix().toLower();
    if (!info.isFile() || (suffix != "exe" && suffix != "lnk")) return {};
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

class ExecutableOpener final : public QDialog {
public:
    explicit ExecutableOpener(const QString &launcher, QWidget *parent = nullptr)
        : QDialog(parent), launcherPath(launcher) {
        setObjectName("executableOpener");
        setWindowTitle("WinBridge");
        setWindowIcon(resolveIcon());
        setMinimumSize(720, 560);
        resize(780, 620);

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
        root->addWidget(topNav);

        auto *content = new QVBoxLayout;
        content->setContentsMargins(28, 22, 28, 24);
        content->setSpacing(16);
        root->addLayout(content, 1);

        auto *intro = new QLabel(tr("Open a Windows app"));
        intro->setObjectName("heading");
        content->addWidget(intro);
        auto *introText = new QLabel(tr("Choose an .exe or .lnk file to run with your shared WinBridge environment."));
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
                tr("Windows apps (*.exe *.EXE *.lnk *.LNK);;All files (*)"));
            if (!selected.isEmpty()) pathEdit->setText(selected);
        });
        connect(pathEdit, &QLineEdit::textChanged, this, [this] { updateSelection(); });
        connect(pathEdit, &QLineEdit::returnPressed, this, [this] { openSelected(); });
        connect(list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
            if (item) pathEdit->setText(item->data(Qt::UserRole).toString());
            updateSelection();
        });
        connect(list, &QListWidget::itemDoubleClicked, this, [this] { openSelected(); });
        connect(clearButton, &QToolButton::clicked, this, [this] {
            QFile::remove(recentExecutablesPath());
            populateRecents();
            pathEdit->clear();
        });
        connect(desktopAction, &QAction::triggered, this, [this] { makeShortcut(ShortcutLocation::Desktop); });
        connect(menuAction, &QAction::triggered, this, [this] { makeShortcut(ShortcutLocation::StartMenu); });
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(openButton, &QPushButton::clicked, this, [this] { openSelected(); });

        setStyleSheet(styleSheetText());
        populateRecents();
        updateSelection();
        pathEdit->setFocus();
    }

    QString executable() const { return selectedExecutable; }

private:
    QString launcherPath;
    QString selectedExecutable;
    QLineEdit *pathEdit = nullptr;
    QListWidget *list = nullptr;
    QLabel *emptyLabel = nullptr;
    QLabel *status = nullptr;
    QPushButton *openButton = nullptr;
    QPushButton *shortcutButton = nullptr;
    QToolButton *clearButton = nullptr;

    static QIcon resolveIcon() {
        QIcon icon = QIcon::fromTheme("winbridge");
        if (!icon.isNull()) return icon;
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates = {
            appDir + "/../share/pixmaps/winbridge.png",
            appDir + "/../assets/winbridge.png",
            appDir + "/../../assets/winbridge.png",
            "/usr/local/share/pixmaps/winbridge.png",
            "/usr/share/pixmaps/winbridge.png"
        };
        for (const QString &candidate : candidates) {
            if (QFileInfo(candidate).isFile()) return QIcon(candidate);
        }
        return {};
    }

    void populateRecents() {
        list->clear();
        for (const QString &path : recentExecutables()) {
            QFileInfo info(path);
            auto *item = new QListWidgetItem;
            item->setText(info.completeBaseName() + "\n" + info.dir().absolutePath());
            item->setToolTip(path);
            item->setData(Qt::UserRole, path);
            item->setData(Qt::UserRole + 1, info.dir().absolutePath());
            item->setIcon(QIcon::fromTheme("application-x-executable"));
            item->setSizeHint(QSize(0, 62));
            list->addItem(item);
        }
        const bool hasItems = list->count() > 0;
        list->setVisible(hasItems);
        emptyLabel->setVisible(!hasItems);
        clearButton->setVisible(hasItems);
    }

    void updateSelection() {
        const QString valid = normalizedExecutable(pathEdit->text());
        openButton->setEnabled(!valid.isEmpty());
        shortcutButton->setEnabled(!valid.isEmpty());
        status->setText(valid.isEmpty() ? tr("Select an app or enter its path.") : valid);
    }

    void openSelected() {
        const QString valid = normalizedExecutable(pathEdit->text());
        if (valid.isEmpty()) {
            status->setText(tr("Choose an existing .exe or .lnk file."));
            return;
        }
        selectedExecutable = valid;
        accept();
    }

    void makeShortcut(ShortcutLocation location) {
        const QString valid = normalizedExecutable(pathEdit->text());
        if (valid.isEmpty()) return;
        try {
            const QString created = createExecutableShortcut(valid, launcherPath, location);
            status->setText(location == ShortcutLocation::Desktop
                ? tr("Desktop shortcut created: %1").arg(created)
                : tr("Start Menu shortcut created: %1").arg(created));
        } catch (const std::exception &error) {
            QMessageBox::critical(this, tr("WinBridge"), QString::fromUtf8(error.what()));
        }
    }

    static QString styleSheetText() {
        const bool dark = QSettings("WinBridge", "Manager").value("theme", "classic").toString() == "dark";
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
            QToolButton#clearButton { border: none; color: {{accent}}; padding: 4px; background: transparent; font-weight: bold; }
            QMenu { background: {{face}}; color: {{text}}; border-top: 2px solid {{light}}; border-left: 2px solid {{light}}; border-right: 2px solid {{darkshadow}}; border-bottom: 2px solid {{darkshadow}}; padding: 3px; }
            QMenu::item { padding: 7px 28px 7px 10px; }
            QMenu::item:selected { background: {{selected}}; }
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
};

OpenRequest showExecutableOpener(const QString &launcher, QWidget *parent) {
    ExecutableOpener dialog(launcher, parent);
    OpenRequest request;
    request.accepted = dialog.exec() == QDialog::Accepted;
    request.executable = dialog.executable();
    return request;
}

} // namespace WinBridge
