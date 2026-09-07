#define WINBRIDGE_MANAGER_TEST
#include "main.cpp"
#include <QtTest>

class ManagerTest : public QObject {
    Q_OBJECT
private slots:
    void translationFallback() {
        I18n::load("en-US"); QCOMPARE(T("Settings"), QString("Settings"));
        I18n::load("no-NB"); QCOMPARE(T("Settings"), QString::fromUtf8("Innstillinger"));
        QCOMPARE(T("Untranslated name"),QString("Untranslated name"));
        I18n::load("en-US");
    }
    void settingsLanguagePersists() {
        QTemporaryDir dir;
        QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,dir.path());
        QString script=dir.filePath("backend.py");QFile f(script);QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("import json\nprint(json.dumps({'selected':'','versions':[{'name':'Proton Test','path':'/test/proton'}]}))\n");f.close();
        SettingsDialog dialog(script);dialog.show();
        auto *save=dialog.findChild<QPushButton*>("primary");
        auto *language=dialog.findChild<QComboBox*>("language");
        auto *proton=dialog.findChild<QComboBox*>("proton");
        QTRY_VERIFY_WITH_TIMEOUT(save->isEnabled(),5000);
        QCOMPARE(proton->count(),2);
        language->setCurrentIndex(1);
        save->click();
        QTRY_COMPARE_WITH_TIMEOUT(dialog.result(),int(QDialog::Accepted),5000);
        QCOMPARE(QSettings("WinBridge","Manager").value("language").toString(),QString("no-NB"));
        QSettings("WinBridge","Manager").clear();
    }
    void searchSelectionAndUninstall() {
        QTemporaryDir dir;
        QString script=dir.filePath("backend.py"), calls=dir.filePath("calls");
        QFile file(script); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(("import json,sys\nfrom pathlib import Path\np=Path("+QString("'%1'").arg(calls)+")\np.write_text((p.read_text() if p.exists() else '') + sys.argv[1] + '\\n')\nprint(json.dumps({'prefix':'/tmp/test-winbridge','proton':'Test Proton','ready':True,'removed':True,'programs':([] if sys.argv[1]=='uninstall' else [{'key':'alpha','name':'Alpha App'},{'key':'beta','name':'Beta App'}])}))\n").toUtf8()); file.close();
        Manager window(script); window.show();
        auto *list=window.findChild<QListWidget*>("programList");
        auto *search=window.findChild<QLineEdit*>("search");
        auto *remove=window.findChild<QPushButton*>("danger");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(),2,5000);
        QVERIFY(!remove->isEnabled());
        search->setText("beta"); QCOMPARE(list->count(),1);
        list->setCurrentRow(0); QVERIFY(remove->isEnabled());
        QTimer::singleShot(20,[]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); if(box)box->reject();});
        remove->click();
        QVERIFY(file.open(QIODevice::ReadOnly)==true); file.close();
        QFile callFile(calls); QVERIFY(callFile.open(QIODevice::ReadOnly)); QCOMPARE(callFile.readAll(),QByteArray("list\n"));callFile.close();
        QTimer::singleShot(20,[]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); if(box)for(auto *b:box->buttons())if(box->buttonRole(b)==QMessageBox::AcceptRole)b->click();});
        remove->click();
        QTRY_COMPARE_WITH_TIMEOUT(list->count(),0,5000);
        QVERIFY(callFile.open(QIODevice::ReadOnly)); QCOMPARE(callFile.readAll(),QByteArray("list\nuninstall\n"));
    }
};
QTEST_MAIN(ManagerTest)
#include "test_manager.moc"
