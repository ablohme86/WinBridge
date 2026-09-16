/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "executable_icon.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace WinBridge {
namespace {

quint16 read16(const QByteArray &data, qsizetype offset, bool *ok = nullptr) {
    const bool valid = offset >= 0 && offset + 2 <= data.size();
    if (ok) *ok = valid;
    if (!valid) return 0;
    const auto *p = reinterpret_cast<const uchar *>(data.constData() + offset);
    return quint16(p[0]) | (quint16(p[1]) << 8);
}

quint32 read32(const QByteArray &data, qsizetype offset, bool *ok = nullptr) {
    const bool valid = offset >= 0 && offset + 4 <= data.size();
    if (ok) *ok = valid;
    if (!valid) return 0;
    const auto *p = reinterpret_cast<const uchar *>(data.constData() + offset);
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}

void append16(QByteArray &data, quint16 value) {
    data.append(char(value & 0xff));
    data.append(char((value >> 8) & 0xff));
}

void append32(QByteArray &data, quint32 value) {
    data.append(char(value & 0xff));
    data.append(char((value >> 8) & 0xff));
    data.append(char((value >> 16) & 0xff));
    data.append(char((value >> 24) & 0xff));
}

struct Section {
    quint32 virtualAddress = 0;
    quint32 virtualSize = 0;
    quint32 rawOffset = 0;
    quint32 rawSize = 0;
};

class PeResources {
public:
    explicit PeResources(QByteArray contents) : bytes(std::move(contents)) {}

    bool initialize() {
        bool ok = false;
        if (read16(bytes, 0, &ok) != 0x5a4d || !ok) return false;
        const quint32 peOffset = read32(bytes, 0x3c, &ok);
        if (!ok || read32(bytes, peOffset, &ok) != 0x00004550 || !ok) return false;
        const quint16 sectionCount = read16(bytes, peOffset + 6, &ok);
        const quint16 optionalSize = read16(bytes, peOffset + 20, &ok);
        if (!ok || sectionCount == 0) return false;
        const qsizetype optional = peOffset + 24;
        const quint16 magic = read16(bytes, optional, &ok);
        if (!ok || (magic != 0x10b && magic != 0x20b)) return false;
        const qsizetype dataDirectories = optional + (magic == 0x20b ? 112 : 96);
        resourceRva = read32(bytes, dataDirectories + 16, &ok);
        resourceSize = read32(bytes, dataDirectories + 20, &ok);
        if (!ok || resourceRva == 0 || resourceSize < 16) return false;

        const qsizetype sectionTable = optional + optionalSize;
        for (quint16 i = 0; i < sectionCount; ++i) {
            const qsizetype entry = sectionTable + qsizetype(i) * 40;
            if (entry + 40 > bytes.size()) return false;
            Section section;
            section.virtualSize = read32(bytes, entry + 8);
            section.virtualAddress = read32(bytes, entry + 12);
            section.rawSize = read32(bytes, entry + 16);
            section.rawOffset = read32(bytes, entry + 20);
            sections.append(section);
        }
        const qint64 mapped = fileOffset(resourceRva, 16);
        if (mapped < 0) return false;
        resourceBase = mapped;
        return true;
    }

    QByteArray bestIconAsIco() const {
        quint32 groupDirectory = 0;
        if (!directoryForId(0, 14, groupDirectory)) return {};
        quint32 groupNameDirectory = 0;
        if (!firstDirectory(groupDirectory, groupNameDirectory)) return {};
        QByteArray group = firstData(groupNameDirectory);
        if (group.size() < 20 || read16(group, 0) != 0 || read16(group, 2) != 1) return {};

        const quint16 count = read16(group, 4);
        int best = -1;
        quint64 bestScore = 0;
        for (quint16 i = 0; i < count; ++i) {
            const qsizetype offset = 6 + qsizetype(i) * 14;
            if (offset + 14 > group.size()) break;
            const quint32 width = uchar(group[offset]) == 0 ? 256 : uchar(group[offset]);
            const quint32 height = uchar(group[offset + 1]) == 0 ? 256 : uchar(group[offset + 1]);
            const quint32 bits = read16(group, offset + 6);
            const quint32 size = read32(group, offset + 8);
            const quint64 score = quint64(width) * height * 65536ULL + quint64(bits) * 256ULL + std::min(size, quint32(255));
            if (score > bestScore) {
                bestScore = score;
                best = i;
            }
        }
        if (best < 0) return {};

        const qsizetype groupEntry = 6 + qsizetype(best) * 14;
        const quint16 iconId = read16(group, groupEntry + 12);
        quint32 iconDirectory = 0;
        if (!directoryForId(0, 3, iconDirectory)) return {};
        quint32 iconLanguageDirectory = 0;
        if (!directoryForId(iconDirectory, iconId, iconLanguageDirectory)) return {};
        const QByteArray image = firstData(iconLanguageDirectory);
        if (image.isEmpty()) return {};

        QByteArray ico;
        ico.reserve(22 + image.size());
        append16(ico, 0);
        append16(ico, 1);
        append16(ico, 1);
        ico.append(group.constData() + groupEntry, 8);
        append32(ico, image.size());
        append32(ico, 22);
        ico.append(image);
        return ico;
    }

private:
    QByteArray bytes;
    QList<Section> sections;
    quint32 resourceRva = 0;
    quint32 resourceSize = 0;
    qint64 resourceBase = -1;

    qint64 fileOffset(quint32 rva, quint32 size) const {
        for (const Section &section : sections) {
            const quint64 span = std::max(section.virtualSize, section.rawSize);
            if (rva < section.virtualAddress || quint64(rva) + size > quint64(section.virtualAddress) + span) continue;
            const quint64 offset = quint64(section.rawOffset) + (rva - section.virtualAddress);
            if (offset + size <= quint64(bytes.size())) return qint64(offset);
        }
        return -1;
    }

    bool directoryForId(quint32 relativeDirectory, quint16 id, quint32 &childDirectory) const {
        const qint64 directory = resourceBase + relativeDirectory;
        bool ok = false;
        const quint16 named = read16(bytes, directory + 12, &ok);
        const quint16 ids = read16(bytes, directory + 14, &ok);
        if (!ok || quint64(relativeDirectory) + 16 + quint64(named + ids) * 8 > resourceSize) return false;
        for (quint32 i = named; i < quint32(named) + ids; ++i) {
            const qint64 entry = directory + 16 + qint64(i) * 8;
            const quint32 name = read32(bytes, entry, &ok);
            const quint32 target = read32(bytes, entry + 4, &ok);
            if (!ok) return false;
            if (!(name & 0x80000000U) && quint16(name) == id && (target & 0x80000000U)) {
                childDirectory = target & 0x7fffffffU;
                return childDirectory < resourceSize;
            }
        }
        return false;
    }

    bool firstDirectory(quint32 relativeDirectory, quint32 &childDirectory) const {
        const qint64 directory = resourceBase + relativeDirectory;
        bool ok = false;
        const quint32 count = quint32(read16(bytes, directory + 12, &ok)) + read16(bytes, directory + 14, &ok);
        if (!ok || count == 0 || quint64(relativeDirectory) + 16 + quint64(count) * 8 > resourceSize) return false;
        for (quint32 i = 0; i < count; ++i) {
            const quint32 target = read32(bytes, directory + 16 + qint64(i) * 8 + 4, &ok);
            if (!ok) return false;
            if (target & 0x80000000U) {
                childDirectory = target & 0x7fffffffU;
                return childDirectory < resourceSize;
            }
        }
        return false;
    }

    QByteArray firstData(quint32 relativeDirectory, int depth = 0) const {
        if (depth > 4) return {};
        const qint64 directory = resourceBase + relativeDirectory;
        bool ok = false;
        const quint32 count = quint32(read16(bytes, directory + 12, &ok)) + read16(bytes, directory + 14, &ok);
        if (!ok || count == 0 || quint64(relativeDirectory) + 16 + quint64(count) * 8 > resourceSize) return {};
        for (quint32 i = 0; i < count; ++i) {
            const quint32 target = read32(bytes, directory + 16 + qint64(i) * 8 + 4, &ok);
            if (!ok) return {};
            if (target & 0x80000000U) {
                const QByteArray nested = firstData(target & 0x7fffffffU, depth + 1);
                if (!nested.isEmpty()) return nested;
                continue;
            }
            if (quint64(target) + 16 > resourceSize) continue;
            const qint64 dataEntry = resourceBase + target;
            const quint32 rva = read32(bytes, dataEntry, &ok);
            const quint32 size = read32(bytes, dataEntry + 4, &ok);
            if (!ok || size == 0 || size > 64 * 1024 * 1024) continue;
            const qint64 offset = fileOffset(rva, size);
            if (offset >= 0) return bytes.mid(offset, size);
        }
        return {};
    }
};

} // namespace

QString executableIconPath(const QString &exePath, const QString &customCacheDir) {
    QFileInfo info(exePath);
    if (!info.isFile() || info.suffix().compare("exe", Qt::CaseInsensitive) != 0) return {};
    const QString canonical = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
    const QByteArray identity = canonical.toUtf8() + '\0' + QByteArray::number(info.size()) + '\0'
        + QByteArray::number(info.lastModified().toMSecsSinceEpoch());
    const QString key = QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex()).left(24);
    QString cacheDir = customCacheDir;
    if (cacheDir.isEmpty()) {
        cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/executable-icons";
    }
    const QString output = QDir(cacheDir).filePath(key + ".png");
    if (QFileInfo(output).isFile()) return output;

    QFile file(canonical);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > 2LL * 1024 * 1024 * 1024) return {};
    QByteArray ico;
    uchar *mapped = file.map(0, file.size());
    if (mapped) {
        PeResources resources(QByteArray::fromRawData(
            reinterpret_cast<const char *>(mapped), file.size()));
        if (resources.initialize()) ico = resources.bestIconAsIco();
        file.unmap(mapped);
    } else if (file.size() <= 256LL * 1024 * 1024) {
        PeResources resources(file.readAll());
        if (resources.initialize()) ico = resources.bestIconAsIco();
    }
    if (ico.isEmpty()) return {};
    QImage image = QImage::fromData(ico, "ICO");
    if (image.isNull()) return {};
    if (image.width() > 256 || image.height() > 256) {
        image = image.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (!QDir().mkpath(cacheDir)) return {};
    QSaveFile outputFile(output);
    if (!outputFile.open(QIODevice::WriteOnly)) return {};
    if (!image.save(&outputFile, "PNG") || !outputFile.commit()) return {};
    return output;
}

} // namespace WinBridge
