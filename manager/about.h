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
#include "i18n.h"
#include "settings.h"

class AboutDialog : public QDialog {
public:
    AboutDialog(const QString &protonVersion = {}, const QString &prefixPath = {}, QWidget *parent = nullptr)
        : QDialog(parent) {
        setWindowTitle(T("About WinBridge Manager"));
        setWindowIcon(QIcon(":/assets/winbridge.png"));
        setStyleSheet(retroStyleSheet());
        setFixedSize(550, 520);
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(24, 20, 24, 18);
        mainLayout->setSpacing(12);

        // Header: Logo on left, app details on right
        auto *headerLayout = new QHBoxLayout;
        headerLayout->setSpacing(18);
        headerLayout->setAlignment(Qt::AlignTop);

        auto *iconLabel = new QLabel;
        iconLabel->setObjectName("aboutIcon");
        iconLabel->setPixmap(QPixmap(":/assets/winbridge.png").scaled(54, 54, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLabel->setFixedSize(56, 56);
        iconLabel->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);

        auto *detailsLayout = new QVBoxLayout;
        detailsLayout->setSpacing(3);

        auto *title = new QLabel("WinBridge Manager");
        title->setObjectName("aboutTitle");
        detailsLayout->addWidget(title);

        auto *version = new QLabel(T("Version 1.0"));
        version->setObjectName("aboutVersion");
        detailsLayout->addWidget(version);

        auto *tagline = new QLabel(T("Windows Application Manager for Linux"));
        tagline->setObjectName("aboutTagline");
        detailsLayout->addWidget(tagline);

        auto *copyright = new QLabel("Copyright © 2026 A. Blohmè <alexander.blohme@gmail.com>");
        copyright->setObjectName("aboutCopyright");
        detailsLayout->addWidget(copyright);

        auto *license = new QLabel(T("Licensed under the GNU General Public License v3."));
        license->setObjectName("aboutLicense");
        detailsLayout->addWidget(license);

        headerLayout->addLayout(detailsLayout, 1);
        mainLayout->addLayout(headerLayout);

        // 3D Etched Groove Divider
        auto *divider = new QFrame;
        divider->setObjectName("aboutDivider");
        divider->setFrameShape(QFrame::HLine);
        mainLayout->addWidget(divider);

        // Middle: Classic Windows Registered To & Physical Environment Box
        auto *infoBox = new QFrame;
        infoBox->setObjectName("aboutInfoBox");
        auto *infoLayout = new QVBoxLayout(infoBox);
        infoLayout->setSpacing(6);
        infoLayout->setContentsMargins(12, 10, 12, 10);

        auto *regHeading = new QLabel(T("This product is licensed to:"));
        regHeading->setObjectName("aboutHeading");
        infoLayout->addWidget(regHeading);

        auto *userInfo = new QLabel(QString("%1\n%2").arg(T("Everyone!"), T("Free & Open Source Software")));
        userInfo->setObjectName("aboutUserInfo");
        userInfo->setIndent(16);
        infoLayout->addWidget(userInfo);

        auto *gplNotice = new QLabel(
            "This program is free software: you can redistribute it and/or modify\n"
            "it under the terms of the GNU General Public License as published by\n"
            "the Free Software Foundation, either version 3 of the License, or\n"
            "(at your option) any later version.\n\n"
            "This program is distributed in the hope that it will be useful,\n"
            "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
            "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
            "GNU General Public License for more details.\n\n"
            "You should have received a copy of the GNU General Public License\n"
            "along with this program.  If not, see <https://gnu.org>.");
        gplNotice->setObjectName("aboutGplNotice");
        gplNotice->setTextFormat(Qt::PlainText);
        gplNotice->setIndent(16);
        infoLayout->addWidget(gplNotice);

        QString engineDisplay = protonVersion.isEmpty() ? T("Not selected") : protonVersion;
        QString prefixDisplay = prefixPath.isEmpty() ? "~/.local/share/winbridge/shared" : prefixPath;
        if (prefixDisplay.startsWith(QDir::homePath())) {
            prefixDisplay.replace(0, QDir::homePath().length(), "~");
        }

        auto *sysHeading = new QLabel(T("Physical environment:"));
        sysHeading->setObjectName("aboutHeading");
        infoLayout->addWidget(sysHeading);

        auto *sysInfo = new QLabel(QString("Proton: %1\nPrefix: %2").arg(engineDisplay, prefixDisplay));
        sysInfo->setObjectName("aboutSysInfo");
        sysInfo->setIndent(16);
        infoLayout->addWidget(sysInfo);

        mainLayout->addWidget(infoBox, 1);

        // Bottom section: Classic OK button
        auto *buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch();
        auto *okButton = new QPushButton(T("OK"));
        okButton->setObjectName("aboutOk");
        okButton->setFixedWidth(85);
        okButton->setDefault(true);
        connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
        buttonLayout->addWidget(okButton);
        buttonLayout->addStretch();

        mainLayout->addLayout(buttonLayout);
    }
};
