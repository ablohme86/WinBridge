#pragma once
#include <QtWidgets>
#include <QProcess>
#include "i18n.h"

class SettingsDialog : public QDialog {
    QComboBox *language, *proton;
    QLineEdit *prefix;
    QPushButton *browse;
    QLabel *message;
    QPushButton *save, *cancel;
    QProcess *process;
    QString backend, originalLanguage, originalProton, originalPrefix, defaultPrefix;
    bool saving = false, inFlight = false;
    QByteArray output;
    void run(const QStringList &args) {
        inFlight=true; output.clear();save->setEnabled(false);cancel->setEnabled(!saving);
        language->setEnabled(!saving);proton->setEnabled(!saving);
        prefix->setEnabled(!saving);browse->setEnabled(!saving);
        process->start("/usr/bin/python3", QStringList{backend}+args);
    }
    void fail(const QString &error) {
        inFlight=false;message->setText(T(error));cancel->setEnabled(true);
        language->setEnabled(true);proton->setEnabled(true);
        prefix->setEnabled(true);browse->setEnabled(true);save->setEnabled(saving);
    }
    void finish(int code, QProcess::ExitStatus state) {
        output+=process->readAllStandardOutput();
        QJsonParseError parse;
        auto doc=QJsonDocument::fromJson(output,&parse);
        auto result=doc.object();
        if(code!=0 || state!=QProcess::NormalExit || parse.error!=QJsonParseError::NoError || !doc.isObject() || result.contains("error")) {
            fail(result.value("error").toString(saving ? "Could not save settings." : "Could not load settings."));return;
        }
        inFlight=false;
        if(saving) {
            QSettings settings("WinBridge","Manager");settings.setValue("language",language->currentData().toString());settings.sync();
            if(settings.status()!=QSettings::NoError){fail("Could not save settings.");return;}
            accept();return;
        }
        originalProton=result["selected"].toString();
        originalPrefix=result.value("prefix").toString();
        defaultPrefix=result.value("default_prefix").toString();
        if(originalPrefix.isEmpty() && !defaultPrefix.isEmpty()) originalPrefix = defaultPrefix;
        prefix->setText(originalPrefix);
        if(!defaultPrefix.isEmpty()) prefix->setPlaceholderText(defaultPrefix);

        proton->addItem(T(originalProton.isEmpty() ? "Not selected" : "Keep current version"),QString());
        for(const auto &v:result["versions"].toArray()) {
            auto p=v.toObject();proton->addItem(p["name"].toString(),p["path"].toString());
        }
        int selected=proton->findData(originalProton);
        proton->setCurrentIndex(selected<0?0:selected);
        if(proton->count()==1) message->setText(T("No Proton versions found. Install one using Steam."));
        else if(selected<0&&!originalProton.isEmpty()) message->setText(T("Saved version is no longer installed. Please choose another version."));
        else message->setText(T("Close Windows apps before changing Proton versions."));
        save->setEnabled(true);
    }
public:
    bool languageChanged() const {return language->currentData().toString()!=originalLanguage;}
    void reject() override {
        if(saving && inFlight) return;
        if(process->state()!=QProcess::NotRunning){process->kill();process->waitForFinished(1000);}
        QDialog::reject();
    }
    SettingsDialog(const QString &path,QWidget *parent=nullptr):QDialog(parent),backend(path) {
        setWindowTitle(T("Settings"));setMinimumWidth(560);
        auto *layout=new QVBoxLayout(this);layout->setContentsMargins(30,28,30,28);layout->setSpacing(18);
        auto label=[&](const QString &text,const char *id=nullptr){auto *l=new QLabel(T(text));l->setTextFormat(Qt::PlainText);l->setWordWrap(true);if(id)l->setObjectName(id);return l;};
        layout->addWidget(label("Settings","heading"));layout->addWidget(label("Personalize your workspace","muted"));layout->addSpacing(8);
        layout->addWidget(label("Language"));language=new QComboBox;language->setObjectName("language");
        language->addItem("English","en-US");language->addItem("Norsk bokmål","no-NB");
        originalLanguage=QSettings("WinBridge","Manager").value("language","en-US").toString();
        language->setCurrentIndex(qMax(0,language->findData(originalLanguage)));layout->addWidget(language);
        layout->addWidget(label("Choose the language used by WinBridge Manager.","muted"));layout->addSpacing(8);
        layout->addWidget(label("Proton version"));proton=new QComboBox;proton->setObjectName("proton");layout->addWidget(proton);
        layout->addWidget(label("Used by all apps in your shared Windows environment.","muted"));layout->addSpacing(8);

        layout->addWidget(label("Proton / Wine install directory"));
        auto *prefixRow=new QHBoxLayout;prefixRow->setSpacing(8);
        prefix=new QLineEdit;prefix->setObjectName("prefix");prefixRow->addWidget(prefix,1);
        browse=new QPushButton(T("Browse …"));browse->setObjectName("browse");prefixRow->addWidget(browse);
        layout->addLayout(prefixRow);
        layout->addWidget(label("Location of your shared Windows environment and installed apps.","muted"));

        message=label("Loading installed Proton versions …","muted");layout->addWidget(message);
        auto *buttons=new QHBoxLayout;buttons->addStretch();cancel=new QPushButton(T("Cancel"));save=new QPushButton(T("Save changes"));save->setObjectName("primary");save->setEnabled(false);buttons->addWidget(cancel);buttons->addWidget(save);layout->addLayout(buttons);
        process=new QProcess(this);
        connect(process,&QProcess::readyReadStandardOutput,this,[this]{output+=process->readAllStandardOutput();});
        connect(process,&QProcess::readyReadStandardError,this,[this]{process->readAllStandardError();});
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,&SettingsDialog::finish);
        connect(process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart)fail("Could not load settings.");});
        connect(cancel,&QPushButton::clicked,this,&SettingsDialog::reject);
        connect(browse,&QPushButton::clicked,this,[this]{
            QString current=prefix->text().trimmed();
            if(current.isEmpty()) current=defaultPrefix;
            QString startDir=(!current.isEmpty()&&QDir(current).exists())?current:QDir::homePath();
            QString dir=QFileDialog::getExistingDirectory(this,T("Select Proton / Wine directory"),startDir);
            if(!dir.isEmpty())prefix->setText(dir);
        });
        connect(save,&QPushButton::clicked,this,[this]{
            QString chosenProton=proton->currentData().toString();
            QString chosenPrefix=prefix->text().trimmed();
            if(chosenPrefix.isEmpty() && !defaultPrefix.isEmpty()) chosenPrefix = defaultPrefix;
            bool protonChanged=(!chosenProton.isEmpty() && chosenProton!=originalProton);
            bool prefixChanged=(!chosenPrefix.isEmpty() && !originalPrefix.isEmpty() && chosenPrefix!=originalPrefix);
            if(protonChanged || prefixChanged) {
                QString warnTitle = (protonChanged && !prefixChanged) ? T("Change Proton version?") : T("Change installation directory?");
                QString warnText = (protonChanged && !prefixChanged)
                    ? T("Make sure all Windows apps are closed before continuing. The existing Windows environment will be kept.")
                    : T("Make sure all Windows apps are closed before continuing. If you moved your existing Windows environment, existing apps and data will be loaded from the new location.");
                QMessageBox box(QMessageBox::Question,warnTitle,warnText,QMessageBox::NoButton,this);
                auto *no=box.addButton(T("Cancel"),QMessageBox::RejectRole);auto *yes=box.addButton(T("Continue"),QMessageBox::AcceptRole);box.setDefaultButton(no);box.exec();if(box.clickedButton()!=yes)return;
            }
            saving=true;message->setText(T("Saving settings …"));
            QStringList args{"configure"};
            args << "--proton" << chosenProton;
            if(!chosenPrefix.isEmpty()) args << "--prefix" << chosenPrefix;
            run(args);
        });
        run({"settings"});
    }
};
