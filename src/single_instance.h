/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include <memory>

class QLocalServer;
class QWindow;
class QWidget;

namespace WinBridge {

QString activationTokenForWindow(QWindow *window, const QString &targetAppId);
bool activateRunningInstance(const QString &applicationId, const QString &activationToken = {}, int timeoutMs = 250);
void bringWindowToForeground(QWidget *window, const QString &activationToken = {});

class InstanceActivationServer final : public QObject {
public:
    InstanceActivationServer(const QString &applicationId, std::function<void(const QString &)> activation, QObject *parent = nullptr);
    ~InstanceActivationServer() override;
    bool start();

private:
    QString applicationId;
    std::function<void(const QString &)> activation;
    std::unique_ptr<QLocalServer> server;
};

} // namespace WinBridge
