/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "tray.h"
#include "manager_backend.h"
#include "opener.h"
#include "shared_space.h"
#include "single_instance.h"

#include <QtWidgets>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonObject>

#ifdef WINBRIDGE_HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#endif

namespace WinBridge {

QString trayPrefix(const QString &prefixArg) {
    QString prefix = prefixArg.isEmpty() ? sharedPrefix() : prefixArg;
    if (prefix.startsWith("~/")) prefix = QDir::homePath() + prefix.mid(1);
    // launch() stores the canonical compatdata path in STEAM_COMPAT_DATA_PATH,
    // which is what getRunningTasks() compares against.
    const QFileInfo info(prefix);
    return info.canonicalFilePath().isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : info.canonicalFilePath();
}

bool raiseTaskWindow(const QList<qint64> &pids, const QString &exeName) {
#ifdef WINBRIDGE_HAVE_X11
    // Wine windows are X11 clients (XWayland on Wayland sessions), so the
    // EWMH pager request works on both KDE X11 and KDE Wayland.
    Display *display = XOpenDisplay(nullptr);
    if (!display) return false;

    const Atom netClientList = XInternAtom(display, "_NET_CLIENT_LIST", True);
    const Atom netPid = XInternAtom(display, "_NET_WM_PID", True);
    const Atom netActive = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    const QString wantedClass = exeName.toLower();

    Window target = 0;
    Atom actualType;
    int actualFormat;
    unsigned long numItems = 0, bytesAfter = 0;
    unsigned char *prop = nullptr;
    if (netClientList != None &&
        XGetWindowProperty(display, DefaultRootWindow(display), netClientList, 0, 4096, False, XA_WINDOW,
                           &actualType, &actualFormat, &numItems, &bytesAfter, &prop) == Success && prop) {
        Window *windows = reinterpret_cast<Window*>(prop);
        const unsigned long windowCount = numItems;
        for (unsigned long i = 0; i < windowCount; ++i) {
            const Window w = windows[i];
            XWindowAttributes attr;
            if (!XGetWindowAttributes(display, w, &attr) || attr.width < 10 || attr.height < 10) continue;

            bool matched = false;
            unsigned char *pidProp = nullptr;
            unsigned long pidItems = 0;
            if (netPid != None &&
                XGetWindowProperty(display, w, netPid, 0, 1, False, XA_CARDINAL,
                                   &actualType, &actualFormat, &pidItems, &bytesAfter, &pidProp) == Success && pidProp) {
                if (pidItems > 0) matched = pids.contains(static_cast<qint64>(*reinterpret_cast<unsigned long*>(pidProp)));
                XFree(pidProp);
            }
            if (!matched) {
                XClassHint hint;
                if (XGetClassHint(display, w, &hint)) {
                    const QString resName = hint.res_name ? QString::fromUtf8(hint.res_name).toLower() : QString();
                    if (hint.res_name) XFree(hint.res_name);
                    if (hint.res_class) XFree(hint.res_class);
                    matched = !wantedClass.isEmpty() && resName == wantedClass;
                }
            }
            // _NET_CLIENT_LIST is in mapping order; the last match is the
            // most recently opened window of the app.
            if (matched) target = w;
        }
        XFree(prop);
    }

    if (target) {
        XEvent event{};
        event.xclient.type = ClientMessage;
        event.xclient.window = target;
        event.xclient.message_type = netActive;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 2; // Source: pager, so focus stealing prevention allows it.
        event.xclient.data.l[1] = CurrentTime;
        XSendEvent(display, DefaultRootWindow(display), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &event);
        XFlush(display);
    }
    XCloseDisplay(display);
    return target != 0;
#else
    Q_UNUSED(pids)
    Q_UNUSED(exeName)
    return false;
#endif
}

namespace {

class RunningAppsTray final : public QObject {
public:
    explicit RunningAppsTray(const QString &prefix) : prefix(prefix) {
        tray.setIcon(launcherIcon());
        tray.setContextMenu(&menu);
        connect(&menu, &QMenu::aboutToShow, this, [this] { refresh(); });
        connect(&tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) menu.popup(QCursor::pos());
        });
        connect(&pollTimer, &QTimer::timeout, this, [this] { refresh(); });
        pollTimer.start(2000);
        startedAt.start();
        refresh();
    }

private:
    static constexpr int startupTimeoutMs = 5 * 60 * 1000;
    static constexpr int emptyPollsBeforeExit = 3;

    void refresh() {
        tasks = getRunningTasks(prefix);
        if (tasks.isEmpty()) {
            tray.hide();
            // Keep waiting while Proton starts the first app; once apps have
            // been seen, a short grace period covers apps that restart.
            if ((seenTasks && ++emptyPolls >= emptyPollsBeforeExit) ||
                (!seenTasks && startedAt.elapsed() >= startupTimeoutMs)) {
                QCoreApplication::quit();
            }
            return;
        }
        seenTasks = true;
        emptyPolls = 0;
        tray.setToolTip(tasks.size() == 1 ? tr("WinBridge – 1 app running")
                                          : tr("WinBridge – %1 apps running").arg(tasks.size()));
        tray.show();
        if (!menu.isVisible()) rebuildMenu();
    }

    void rebuildMenu() {
        menu.clear();
        auto *title = menu.addAction(tr("Running apps"));
        title->setEnabled(false);
        for (const auto &value : tasks) {
            const QJsonObject task = value.toObject();
            const QString iconPath = task.value("icon").toString();
            const QIcon icon = iconPath.isEmpty() ? launcherIcon() : QIcon(iconPath);
            auto *action = menu.addAction(icon, task.value("name").toString());
            QList<qint64> pids;
            for (const auto &pid : task.value("pids").toArray()) pids.append(pid.toInteger());
            const QString name = task.value("name").toString();
            connect(action, &QAction::triggered, this, [pids, name] { raiseTaskWindow(pids, name); });
        }
        menu.addSeparator();
        auto *killAll = menu.addAction(QIcon::fromTheme("process-stop"), tr("Kill All"));
        connect(killAll, &QAction::triggered, this, [this] { confirmKillAll(); });
    }

    void confirmKillAll() {
        QMessageBox box(QMessageBox::Warning, tr("WinBridge"),
                        tr("Close all running WinBridge apps?\nUnsaved work in these apps will be lost."));
        box.setWindowIcon(launcherIcon());
        auto *cancel = box.addButton(tr("Cancel"), QMessageBox::RejectRole);
        auto *kill = box.addButton(tr("Kill All"), QMessageBox::DestructiveRole);
        box.setDefaultButton(cancel);
        box.exec();
        if (box.clickedButton() != kill) return;
        killAllTasks(prefix);
        QTimer::singleShot(500, this, [this] { refresh(); });
    }

    QString prefix;
    QSystemTrayIcon tray;
    QMenu menu;
    QTimer pollTimer;
    QElapsedTimer startedAt;
    QJsonArray tasks;
    bool seenTasks = false;
    int emptyPolls = 0;
};

} // namespace

int runTray(const QString &prefixArg) {
    if (!qobject_cast<QApplication*>(QCoreApplication::instance())) return 1;
    const QString prefix = trayPrefix(prefixArg);
    const QString instanceId = "tray-" + QString::fromLatin1(
        QCryptographicHash::hash(prefix.toUtf8(), QCryptographicHash::Sha256).toHex().left(12));
    // Every launch starts a tray; only the first one per prefix stays.
    InstanceActivationServer instance(instanceId, [](const QString &) {});
    if (!instance.start()) return 0;
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return 0;
    QApplication::setQuitOnLastWindowClosed(false);
    RunningAppsTray tray(prefix);
    return QCoreApplication::exec();
}

} // namespace WinBridge
