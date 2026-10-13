#pragma once
#include <QList>
#include <QMainWindow>
#include <QString>
#include <QStringList>

#include "languages.h"

extern "C" {
#include "kit_core.h"
}

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class SidePanel;

/* Assistant de désinstallation : confirmation -> suppression -> fin. */
class UninstallerWindow : public QMainWindow {
    Q_OBJECT
public:
    UninstallerWindow(const QString &installDir, const KitManifest &manifest, const Translator &tr);

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onNext();

private:
    enum Page { Confirm, Progress, Done, PageCount };
    QWidget *makePage(const QString &title, const QString &subtitle, QWidget *body);
    void goTo(int page);
    void updateUi();
    void run();

    QString m_dir;
    KitManifest m_m;
    Translator T;
    QStringList m_files;
    int m_cur = Confirm;
    bool m_busy = false;

    QStackedWidget *m_pages = nullptr;
    QList<QLabel *> m_stepLabels;
    QPushButton *m_cancelBtn = nullptr, *m_nextBtn = nullptr;
    QProgressBar *m_bar = nullptr;
    QLabel *m_status = nullptr, *m_doneLabel = nullptr;
    QPlainTextEdit *m_log = nullptr;
};
