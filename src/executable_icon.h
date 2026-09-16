/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QString>

namespace WinBridge {

// Extracts the highest-quality icon embedded in a Windows PE executable and
// caches it as a PNG. Returns an empty string when no usable icon is present.
QString executableIconPath(const QString &exePath, const QString &customCacheDir = "");

} // namespace WinBridge
