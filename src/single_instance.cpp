/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "single_instance.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QDebug>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#ifdef WINBRIDGE_KWINDOWSYSTEM
#include <KWindowSystem>
#include <KWaylandExtras>
#endif
#include <unistd.h>

namespace WinBridge {

static QString serverName(const QString &applicationId) {
    return QString("winbridge-%1-%2-v1").arg(qint64(::geteuid())).arg(applicationId);
}

QString activationTokenForWindow(QWindow *window, const QString &targetAppId) {
#ifdef WINBRIDGE_KWINDOWSYSTEM
    if (window && KWindowSystem::isPlatformWayland()) {
        QFutureWatcher<QString> watcher;
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&watcher, &QFutureWatcher<QString>::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        watcher.setFuture(KWaylandExtras::xdgActivationToken(window, KWaylandExtras::lastInputSerial(window), targetAppId));
        timeout.start(500);
        loop.exec();
        if (watcher.isFinished()) return watcher.result();
    }
#else
    Q_UNUSED(window)
    Q_UNUSED(targetAppId)
#endif
    return qEnvironmentVariable("XDG_ACTIVATION_TOKEN");
}

bool activateRunningInstance(const QString &applicationId, const QString &activationToken, int timeoutMs) {
    QLocalSocket socket;
    socket.connectToServer(serverName(applicationId), QIODevice::WriteOnly);
    if (!socket.waitForConnected(timeoutMs)) return false;
    socket.write("activate\n" + activationToken.toUtf8());
    socket.flush();
    socket.waitForBytesWritten(timeoutMs);
    socket.disconnectFromServer();
    return true;
}

void bringWindowToForeground(QWidget *window, const QString &activationToken) {
    if (!window) return;
    if (window->isMinimized()) window->showNormal();
    else window->show();
    window->raise();
    window->activateWindow();
    if (!window->windowHandle()) return;
#ifdef WINBRIDGE_KWINDOWSYSTEM
    if (!activationToken.isEmpty()) KWindowSystem::setCurrentXdgActivationToken(activationToken);
    KWindowSystem::activateWindow(window->windowHandle());
#endif
    window->windowHandle()->requestActivate();
}

InstanceActivationServer::InstanceActivationServer(
    const QString &applicationId,
    std::function<void(const QString &)> activation,
    QObject *parent
) : QObject(parent), applicationId(applicationId), activation(std::move(activation)), server(std::make_unique<QLocalServer>()) {
    connect(server.get(), &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = server->nextPendingConnection()) {
            const auto handleMessage = [this, socket] {
                const QByteArray message = socket->readAll();
                if (message.startsWith("activate") && this->activation) {
                    const int separator = message.indexOf('\n');
                    this->activation(QString::fromUtf8(separator >= 0 ? message.mid(separator + 1) : QByteArray()));
                }
                socket->disconnectFromServer();
            };
            connect(socket, &QLocalSocket::readyRead, socket, handleMessage);
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            if (socket->bytesAvailable() > 0) handleMessage();
        }
    });
}

InstanceActivationServer::~InstanceActivationServer() = default;

bool InstanceActivationServer::start() {
    const QString name = serverName(applicationId);
    if (server->listen(name)) return true;
    if (server->serverError() != QAbstractSocket::AddressInUseError) {
        qWarning() << "Could not create WinBridge activation server" << name << server->errorString();
        return false;
    }
    if (activateRunningInstance(applicationId, {}, 100)) return false;
    QLocalServer::removeServer(name);
    return server->listen(name);
}

} // namespace WinBridge
