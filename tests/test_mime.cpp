#include <QtTest>
#include <QMimeDatabase>
#include <QProcess>

class MimeTests : public QObject {
    Q_OBJECT
private slots:
    void executableFilesUseAssociations() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString data = tmp.filePath("data");
        const QString mimeDir = data + "/mime";
        QVERIFY(QDir().mkpath(mimeDir + "/packages"));
        QVERIFY(QFile::copy(QStringLiteral(WINBRIDGE_MIME_SOURCE),
                            mimeDir + "/packages/winbridge-exe.xml"));
        qputenv("XDG_DATA_HOME", data.toUtf8());
        QCOMPARE(QProcess::execute(QStringLiteral(UPDATE_MIME_DATABASE), {mimeDir}), 0);

        QMimeDatabase db;
        const QString expected = "application/x-winbridge-exe";
        // MZ content reproduces Qt choosing x-msdownload despite a higher-weight
        // custom glob. Lower-weight competing EXE globs must also be removed.
        QByteArray dos(512, '\0');
        dos[0] = 'M';
        dos[1] = 'Z';
        for (const QString &suffix : {"exe", "EXE", "ExE"}) {
            for (bool executable : {false, true}) {
                const QString path = tmp.filePath("program." + suffix);
                QFile file(path);
                QVERIFY(file.open(QIODevice::WriteOnly));
                QCOMPARE(file.write(dos), qint64(dos.size()));
                file.close();
                auto permissions = QFile::ReadOwner | QFile::WriteOwner;
                if (executable) permissions |= QFile::ExeOwner;
                QVERIFY(file.setPermissions(permissions));
                const QMimeType type = db.mimeTypeForFile(path);
                QCOMPARE(type.name(), expected);
                // These are the inheritance checks KIO uses for direct execution.
                QVERIFY(!type.inherits("application/x-executable"));
                QVERIFY(!type.inherits("application/x-ms-dos-executable"));
            }
        }
        const QString link = tmp.filePath("shortcut.exe");
        QVERIFY(QFile::link(tmp.filePath("program.exe"), link));
        QCOMPARE(db.mimeTypeForFile(link).name(), expected);

        // Native Linux programs and DLLs must retain their original handling.
        const QString self = QCoreApplication::applicationFilePath();
        QVERIFY(db.mimeTypeForFile(self).inherits("application/x-executable"));
        QFile dll(tmp.filePath("library.dll"));
        QVERIFY(dll.open(QIODevice::WriteOnly));
        QCOMPARE(dll.write(dos), qint64(dos.size()));
        dll.close();
        QCOMPARE(db.mimeTypeForFile(dll.fileName()).name(), QString("application/x-msdownload"));
    }
};

QTEST_GUILESS_MAIN(MimeTests)
#include "test_mime.moc"
