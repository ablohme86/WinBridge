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

#define WINBRIDGE_MANAGER_TEST
#include <QtWidgets>
#include <QtTest>
#include "main.cpp"

class FileUrlRecorder : public QObject {
    Q_OBJECT
public:
    QUrl opened;
public slots:
    void record(const QUrl &url) { opened = url; }
};

class ManagerTest : public QObject {
    Q_OBJECT
private slots:
    void showFilesOpensProgramDirectory() {
        QTemporaryDir dir;
        QString installPath = dir.filePath("My App");
        QVERIFY(QDir().mkpath(installPath));
        QString script=dir.filePath("backend.py");
        QFile file(script);QVERIFY(file.open(QIODevice::WriteOnly));
        QJsonArray apps{QJsonObject{{"key","a"},{"name","App"},{"install_path",installPath}},QJsonObject{{"key","b"},{"name","Unknown folder"}}};
        QJsonObject data{{"ready",true},{"proton","Test"},{"prefix",dir.path()},{"programs",apps}};
        file.write("print("+QJsonDocument(QJsonArray{QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))}).toJson(QJsonDocument::Compact).mid(1).chopped(1)+")");file.close();
        FileUrlRecorder recorder;
        QDesktopServices::setUrlHandler("file", &recorder, "record");
        auto cleanup = qScopeGuard([] { QDesktopServices::unsetUrlHandler("file"); });
        Manager window(script);window.show();
        auto *list=window.findChild<QListWidget*>("programList");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(),2,5000);
        QVERIFY(list->findChildren<QAbstractButton*>().isEmpty());
        auto *files=window.findChild<QPushButton*>("showFilesButton");
        QVERIFY(files);QVERIFY(!files->isEnabled());
        list->setCurrentRow(1);QVERIFY(!files->isEnabled());
        list->setCurrentRow(0);QVERIFY(files->isEnabled());
        QCOMPARE(files->text(),QString("Show files"));
        files->click();QCOMPARE(recorder.opened.toLocalFile(),installPath);
        QCOMPARE(list->currentRow(),0);

        // A context menu must act on the clicked row, even if another is selected.
        list->setCurrentRow(1);recorder.opened=QUrl();
        QPoint position=list->visualItemRect(list->item(0)).center();
        QContextMenuEvent event(QContextMenuEvent::Mouse,position,list->viewport()->mapToGlobal(position));
        QApplication::sendEvent(list->viewport(),&event);
        auto *context=window.findChild<QMenu*>("programContextMenu");QVERIFY(context);
        QCOMPARE(list->currentRow(),0);
        auto *open=context->findChild<QAction*>("contextShowFiles");QVERIFY(open && open->isEnabled());
        open->trigger();context->close();
        QCOMPARE(recorder.opened.toLocalFile(),installPath);
        QVERIFY(QDir().rmdir(installPath));recorder.opened=QUrl();
        files->click();QVERIFY(recorder.opened.isEmpty());
    }
    void protonDownloadPreservesEditsAndRecoversFromFailure() {
        QTemporaryDir dir;
        QString script=dir.filePath("backend.py");
        QFile f(script);QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"PY(import json,sys,time
from pathlib import Path
data={'selected':'','prefix':'/original','umu_available':True,'versions':[{'name':'GE-Proton (automatic download)','path':'GE-Proton'},{'name':'UMU-Proton (automatic download)','path':'UMU-Proton'}]}
if sys.argv[1]=='install_proton':
    time.sleep(0.1)
    assert sys.argv[2:] == ['--proton','GE-Proton']
    marker=Path(__file__).with_suffix('.attempt')
    if not marker.exists():
        marker.touch()
        print(json.dumps({'error':'Download failed. Log: /tmp/download.log'}))
        sys.exit(1)
    data['versions'].append({'name':'GE-Proton10-1','path':'/downloaded/GE-Proton10-1'})
print(json.dumps(data))
)PY");f.close();
        SettingsDialog dialog(script);dialog.show();
        auto *download=dialog.findChild<QPushButton*>("downloadProton");
        auto *save=dialog.findChild<QPushButton*>("primary");
        auto *prefix=dialog.findChild<QLineEdit*>("prefix");
        auto *proton=dialog.findChild<QComboBox*>("proton");
        auto *progress=dialog.findChild<QProgressBar*>("downloadProgress");
        QTRY_VERIFY_WITH_TIMEOUT(download->isEnabled(),5000);
        prefix->setText("/unsaved prefix");
        download->click();
        QVERIFY(progress->isVisible());QVERIFY(!save->isEnabled());QVERIFY(!download->isEnabled());
        dialog.reject();QVERIFY(dialog.isVisible());
        QTRY_VERIFY_WITH_TIMEOUT(download->isEnabled(),5000);
        QVERIFY(!progress->isVisible());QVERIFY(save->isEnabled());
        QCOMPARE(prefix->text(),QString("/unsaved prefix"));
        QCOMPARE(proton->count(),3);
        download->click();
        QTRY_VERIFY_WITH_TIMEOUT(download->isEnabled(),5000);
        QCOMPARE(prefix->text(),QString("/unsaved prefix"));
        QCOMPARE(proton->count(),4);
        QCOMPARE(proton->currentData().toString(),QString("GE-Proton"));
        QVERIFY(!progress->isVisible());QVERIFY(dialog.isVisible());
        dialog.reject();QVERIFY(!dialog.isVisible());
    }
    void programIconAndFallback() {
        QTemporaryDir dir;
        QString script=dir.filePath("backend.py");
        QString icon=dir.filePath("app.png");
        QPixmap pixmap(64,64);pixmap.fill(Qt::green);QVERIFY(pixmap.save(icon));
        QFile file(script);QVERIFY(file.open(QIODevice::WriteOnly));
        QJsonArray apps{QJsonObject{{"key","a"},{"name","App"},{"icon",icon}},QJsonObject{{"key","b"},{"name","Blank"},{"icon","/missing/icon.png"}}};
        QJsonObject data{{"ready",true},{"proton","Test"},{"prefix",dir.path()},{"programs",apps}};
        file.write("print("+QJsonDocument(QJsonArray{QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))}).toJson(QJsonDocument::Compact).mid(1).chopped(1)+")");file.close();
        Manager window(script);window.show();
        auto *list=window.findChild<QListWidget*>("programList");
        auto *badge=window.findChild<QLabel*>("badge");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(),2,5000);
        list->setCurrentRow(0);QVERIFY(!badge->pixmap().isNull());
        list->setCurrentRow(1);QVERIFY(badge->pixmap().isNull());QCOMPARE(badge->text(),QString("B"));
    }
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
    void settingsThemePersists() {
        QTemporaryDir dir;
        QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,dir.path());
        QString script=dir.filePath("backend.py");QFile f(script);QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("import json\nprint(json.dumps({'selected':'','versions':[{'name':'Proton Test','path':'/test/proton'}]}))\n");f.close();
        SettingsDialog dialog(script);dialog.show();
        auto *save=dialog.findChild<QPushButton*>("primary");
        auto *theme=dialog.findChild<QComboBox*>("theme");
        QVERIFY(theme!=nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(save->isEnabled(),5000);
        QCOMPARE(theme->count(),2);
        dialog.grab().save("/tmp/settings_light.png");
        theme->setCurrentIndex(1);
        dialog.grab().save("/tmp/settings_dark.png");
        save->click();
        QTRY_COMPARE_WITH_TIMEOUT(dialog.result(),int(QDialog::Accepted),5000);
        QCOMPARE(QSettings("WinBridge","Manager").value("theme").toString(),QString("dark"));
        QSettings("WinBridge","Manager").clear();
    }
    void settingsPrefixConfiguresBackend() {
        QTemporaryDir dir;
        QString script=dir.filePath("backend.py"), calls=dir.filePath("calls");
        QFile f(script);QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(("import json,sys\nfrom pathlib import Path\np=Path("+QString("'%1'").arg(calls)+")\nargs=' '.join(sys.argv[1:])\np.write_text((p.read_text() if p.exists() else '') + args + '\\n')\nprint(json.dumps({'selected':'/test/proton','prefix':'/test/custom_prefix','default_prefix':'/test/default','versions':[{'name':'Proton Test','path':'/test/proton'}]}))\n").toUtf8());f.close();
        SettingsDialog dialog(script);dialog.show();
        auto *save=dialog.findChild<QPushButton*>("primary");
        auto *prefix=dialog.findChild<QLineEdit*>("prefix");
        auto *browse=dialog.findChild<QPushButton*>("browse");
        QVERIFY(browse!=nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(save->isEnabled(),5000);
        QCOMPARE(prefix->text(),QString("/test/custom_prefix"));
        prefix->setText("/new/moved/prefix");
        QTimer::singleShot(20,[]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()); if(box)for(auto *b:box->buttons())if(box->buttonRole(b)==QMessageBox::AcceptRole)b->click();});
        save->click();
        QTRY_COMPARE_WITH_TIMEOUT(dialog.result(),int(QDialog::Accepted),5000);
        QFile callFile(calls); QVERIFY(callFile.open(QIODevice::ReadOnly));
        QString content = QString::fromUtf8(callFile.readAll());
        QVERIFY(content.contains("configure"));
        QVERIFY(content.contains("--prefix /new/moved/prefix"));
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
    void expandableShortcutToggles() {
        QTemporaryDir dir;
        QString script = dir.filePath("backend.py"), calls = dir.filePath("calls");
        QFile file(script); QVERIFY(file.open(QIODevice::WriteOnly));
        QJsonArray scs{QJsonObject{{"id","sc1"},{"name","Game"},{"icon",""},{"desktop",true},{"menu",true}}};
        QJsonArray apps{QJsonObject{{"key","game1"},{"name","Game App"},{"shortcuts",scs}}};
        QJsonObject data{{"ready",true},{"prefix",dir.path()},{"programs",apps}};
        file.write(("import json,sys\nfrom pathlib import Path\np=Path("+QString("'%1'").arg(calls)+")\nargs=' '.join(sys.argv[1:])\np.write_text((p.read_text() if p.exists() else '') + args + '\\n')\nprint("+QJsonDocument(QJsonArray{QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))}).toJson(QJsonDocument::Compact).mid(1).chopped(1)+")\n").toUtf8()); file.close();
        Manager window(script); window.show();
        auto *list = window.findChild<QListWidget*>("programList");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1, 5000);
        list->setCurrentRow(0);
        auto *toggle = window.findChild<QPushButton*>("shortcutToggle");
        QVERIFY(toggle != nullptr);
        auto *panel = window.findChild<QFrame*>("shortcutsPanel");
        QVERIFY(panel != nullptr);
        QVERIFY(!panel->isVisible());
        toggle->click();
        QVERIFY(panel->isVisible());
        auto *cbDesktop = panel->findChild<QCheckBox*>("cbDesktop");
        QVERIFY(cbDesktop != nullptr && cbDesktop->isChecked());
        cbDesktop->setChecked(false);
        QTRY_VERIFY_WITH_TIMEOUT([&]() {
            QFile f(calls);
            return f.open(QIODevice::ReadOnly) && f.readAll().contains("toggle_shortcut");
        }(), 5000);
        QFile callFile(calls);
        QVERIFY(callFile.open(QIODevice::ReadOnly));
        QString recorded = QString::fromUtf8(callFile.readAll());
        QVERIFY(recorded.contains("toggle_shortcut"));
        QVERIFY(recorded.contains("--id sc1"));
        QVERIFY(recorded.contains("--desktop 0"));
    }
    void detectRunningAndKillButton() {
        QTemporaryDir dir;
        QString script = dir.filePath("backend.py"), calls = dir.filePath("calls");
        QFile file(script); QVERIFY(file.open(QIODevice::WriteOnly));
        QJsonArray apps{
            QJsonObject{{"key","run1"},{"name","Running Game"},{"running",true},{"pids",QJsonArray{1234}}},
            QJsonObject{{"key","stop1"},{"name","Stopped Game"},{"running",false},{"pids",QJsonArray{}}}
        };
        QJsonObject data{{"ready",true},{"prefix",dir.path()},{"programs",apps}};
        file.write(("import json,sys\nfrom pathlib import Path\np=Path("+QString("'%1'").arg(calls)+")\nargs=' '.join(sys.argv[1:])\np.write_text((p.read_text() if p.exists() else '') + args + '\\n')\nif 'kill' in sys.argv:\n    print(json.dumps({'killed': True, 'key': sys.argv[sys.argv.index('--key')+1]}))\nelif 'running' in sys.argv:\n    print(json.dumps({'running': {'run1': True, 'stop1': False}}))\nelse:\n    print("+QJsonDocument(QJsonArray{QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))}).toJson(QJsonDocument::Compact).mid(1).chopped(1)+")\n").toUtf8()); file.close();
        Manager window(script); window.show();
        auto *list = window.findChild<QListWidget*>("programList");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 2, 5000);

        auto *runningBadge = window.findChild<QLabel*>("running");
        QVERIFY(runningBadge != nullptr);
        QVERIFY(runningBadge->text().contains("Running") || runningBadge->text().contains("Kjører"));

        QVERIFY(list->findChildren<QAbstractButton*>().isEmpty());

        auto *detailKill = window.findChild<QPushButton*>("killAppButton");
        QVERIFY(detailKill != nullptr);

        list->setCurrentRow(0);
        QVERIFY(detailKill->isEnabled());

        list->setCurrentRow(1);
        QVERIFY(!detailKill->isEnabled());

        list->setCurrentRow(0);
        QTimer::singleShot(20, []() {
            auto *box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (box) {
                for (auto *b : box->buttons()) {
                    if (box->buttonRole(b) == QMessageBox::AcceptRole) {
                        b->click();
                        break;
                    }
                }
            }
        });
        detailKill->click();

        QTRY_VERIFY_WITH_TIMEOUT([&]() {
            QFile f(calls);
            return f.open(QIODevice::ReadOnly) && f.readAll().contains("kill");
        }(), 5000);

        QFile callFile(calls);
        QVERIFY(callFile.open(QIODevice::ReadOnly));
        QString recorded = QString::fromUtf8(callFile.readAll());
        QVERIFY(recorded.contains("kill"));
        QVERIFY(recorded.contains("--key run1"));
    }
    void contextMenuUninstall() {
        QTemporaryDir dir;
        QString script = dir.filePath("backend.py"), calls = dir.filePath("calls");
        QFile file(script); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(("import json,sys\nfrom pathlib import Path\np=Path("+QString("'%1'").arg(calls)+")\nargs=' '.join(sys.argv[1:])\np.write_text((p.read_text() if p.exists() else '') + args + '\\n')\nprint(json.dumps({'prefix':'" + dir.path() + "','proton':'Test','ready':True,'removed':True,'programs':([] if sys.argv[1]=='uninstall' else [{'key':'alpha','name':'Alpha App'}])}))\n").toUtf8()); file.close();
        Manager window(script); window.show();
        auto *list = window.findChild<QListWidget*>("programList");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1, 5000);
        QVERIFY(list->findChildren<QAbstractButton*>().isEmpty());
        QPoint position=list->visualItemRect(list->item(0)).center();
        QContextMenuEvent event(QContextMenuEvent::Mouse,position,list->viewport()->mapToGlobal(position));
        QApplication::sendEvent(list->viewport(),&event);
        auto *context=window.findChild<QMenu*>("programContextMenu");QVERIFY(context);
        auto *uninstall=context->findChild<QAction*>("contextUninstall");QVERIFY(uninstall && uninstall->isEnabled());
        auto *files=context->findChild<QAction*>("contextShowFiles");QVERIFY(files && !files->isEnabled());
        QTimer::singleShot(20, []() {
            auto *box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (box) {
                for (auto *b : box->buttons()) {
                    if (box->buttonRole(b) == QMessageBox::AcceptRole) {
                        b->click();
                        break;
                    }
                }
            }
        });
        context->hide();uninstall->trigger();context->close();
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 0, 5000);
        QFile callFile(calls);
        QVERIFY(callFile.open(QIODevice::ReadOnly));
        QString recorded = QString::fromUtf8(callFile.readAll());
        QVERIFY(recorded.contains("uninstall"));
        QVERIFY(recorded.contains("--key alpha"));
    }
    void aboutDialogOpens() {
        QTemporaryDir dir;
        QString script = dir.filePath("backend.py");
        QFile file(script); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("import json\nprint(json.dumps({'prefix':'/tmp/test','proton':'Test Proton','ready':True,'programs':[{'key':'a','name':'App'}]}))\n");
        file.close();
        Manager window(script); window.show();
        auto *list = window.findChild<QListWidget*>("programList");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1, 5000);
        auto *aboutBtn = window.findChild<QPushButton*>("aboutButton");
        QVERIFY(aboutBtn != nullptr);
        QVERIFY(aboutBtn->isEnabled());
        bool verifiedDialog = false;
        QTimer::singleShot(50, [&]() {
            auto *dlg = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (dlg) {
                verifiedDialog = true;
                dlg->grab().save("/tmp/about_dialog.png");
                auto *okBtn = dlg->findChild<QPushButton*>("aboutOk");
                if (okBtn) okBtn->click();
                else dlg->accept();
            }
        });
        aboutBtn->click();
        QVERIFY(verifiedDialog);
    }
};
QTEST_MAIN(ManagerTest)
#include "test_manager.moc"
