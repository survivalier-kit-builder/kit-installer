#pragma once
#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QString>

#include "languages.h"

extern "C" {
#include "kit_core.h"
}

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTextBrowser;
class SidePanel;
class TitleBar;

class InstallerWindow : public QMainWindow {
    Q_OBJECT
public:
    InstallerWindow(const QString &payloadDir, const KitManifest &manifest);

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onNext();
    void onBack();
    void onCancel();
    void onBrowse();
    void onLanguageRow(int row);

private:
    enum Page { Language, Welcome, License, Destination, Options, Install, Finish, PageCount };

    void loadLanguages();
    void buildUi();
    QWidget *makePage(int page, QWidget *body);
    QWidget *makeLanguage();
    QWidget *makeWelcome();
    QWidget *makeLicense();
    QWidget *makeDestination();
    QWidget *makeOptions();
    QWidget *makeInstall();
    QWidget *makeFinish();

    void setLanguage(const LangInfo &lang);
    void retranslate();
    void goTo(int page);
    void updateUi();
    void runInstall();
    bool doInstall(QString *error);
    void step(const QString &text, int percent);
    void log(const QString &line);
    static int onProgress(double fraction, void *user);

    QString m_payload;
    KitManifest m_m;
    QString m_id, m_exePath;
    int m_cur = Welcome, m_first = Language;
    bool m_busy = false, m_cancelled = false, m_hasLicenseFile = false, m_pathLinked = false, m_confirming = false;
    double m_progBase = 0, m_progWeight = 0;

    QList<LangInfo> m_langs;
    LangInfo m_lang;
    QMap<QString, QString> m_fallback;
    Translator T;

    SidePanel *m_side = nullptr;
    TitleBar *m_titleBar = nullptr;
    QStackedWidget *m_pages = nullptr;
    QList<QLabel *> m_stepLabels;
    QLabel *m_titles[PageCount] = {}, *m_subs[PageCount] = {};
    QPushButton *m_cancelBtn = nullptr, *m_backBtn = nullptr, *m_nextBtn = nullptr, *m_browseBtn = nullptr, *m_keyBtn = nullptr;
    QCheckBox *m_accept = nullptr, *m_menuCheck = nullptr, *m_deskCheck = nullptr, *m_pathCheck = nullptr, *m_launchCheck = nullptr;
    QLineEdit *m_destEdit = nullptr;
    QListWidget *m_langList = nullptr;
    QProgressBar *m_bar = nullptr;
    QLabel *m_status = nullptr, *m_finishLabel = nullptr, *m_welcomeBody = nullptr, *m_destHint = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QTextBrowser *m_licenseText = nullptr;
    QString m_finishPath;
};
