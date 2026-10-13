#include "uninstallerwindow.h"
#include "chrome.h"
#include "diagnostics.h"
#include "sidepanel.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QVBoxLayout>

static QString S(const char *s) { return QString::fromUtf8(s); }

UninstallerWindow::UninstallerWindow(const QString &installDir, const KitManifest &manifest, const Translator &tr)
    : m_dir(installDir), m_m(manifest), T(tr)
{
    const QString name = S(m_m.name);
    setWindowTitle(T("un.window", name));
    setFixedSize(920, 600);

    // Liste des fichiers installés (écrite par l'installeur). On ne supprime que des fichiers/liens listés.
    QFile list(m_dir + "/uninstall.list");
    if (list.open(QIODevice::ReadOnly))
        for (const QString &l : QString::fromUtf8(list.readAll()).split('\n', Qt::SkipEmptyParts)) {
            const QString p = l.trimmed();
            const QFileInfo fi(p);
            if (!p.startsWith('/') || p == "/" || p == QDir::homePath()) continue;      // garde-fous
            if (fi.isDir() && !fi.isSymLink()) continue;                                 // jamais de dossier
            if (fi.isSymLink() || fi.exists()) m_files << p;
        }

    auto *central = new QWidget;
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *side = new SidePanel;
    side->setTitle(T("side.title"));
    side->setProgramName(name);
    side->setVersion(m_m.version[0] ? "v" + S(m_m.version) : QString());
    side->setFooter(m_m.copyright[0] ? S(m_m.copyright)
                                     : QString("© %1 | %2").arg(QDate::currentDate().year())
                                           .arg(m_m.developer[0] ? S(m_m.developer) : name));
    char id[128];
    kit_make_id(m_m.name, id, sizeof id);
    side->setLogo(QPixmap(m_dir + "/" + QString::fromUtf8(id) + ".png"));
    root->addWidget(side);

    auto *right = new QWidget;
    right->setObjectName("rightPane");
    auto *rv = new QVBoxLayout(right);
    rv->setContentsMargins(32, 26, 32, 22);
    rv->setSpacing(16);

    auto *stepper = new QHBoxLayout;
    stepper->setSpacing(6);
    for (int i = 0; i < PageCount; ++i) { m_stepLabels << new QLabel; stepper->addWidget(m_stepLabels.last()); }
    stepper->addStretch();
    rv->addLayout(stepper);

    m_pages = new QStackedWidget;

    // 1) Confirmation
    auto *cw = new QWidget;
    auto *cv = new QVBoxLayout(cw);
    cv->setContentsMargins(0, 0, 0, 0);
    auto *ask = new QLabel(T("un.confirm.text", name));
    ask->setWordWrap(true);
    auto *items = new QPlainTextEdit(m_files.join('\n'));
    items->setReadOnly(true);
    items->setLineWrapMode(QPlainTextEdit::NoWrap);
    cv->addWidget(ask);
    cv->addWidget(items, 1);
    m_pages->addWidget(makePage(T("un.confirm.title", name), T("un.confirm.subtitle"), cw));

    // 2) Suppression
    auto *pw = new QWidget;
    auto *pv = new QVBoxLayout(pw);
    pv->setContentsMargins(0, 0, 0, 0);
    pv->setSpacing(12);
    m_bar = new QProgressBar;
    m_bar->setRange(0, 100);
    m_bar->setTextVisible(false);
    m_status = new QLabel;
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    pv->addWidget(m_bar);
    pv->addWidget(m_status);
    pv->addWidget(m_log, 1);
    m_pages->addWidget(makePage(T("un.progress.title"), T("un.progress.subtitle"), pw));

    // 3) Fin
    m_doneLabel = new QLabel;
    m_doneLabel->setWordWrap(true);
    m_doneLabel->setTextFormat(Qt::RichText);
    m_doneLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_pages->addWidget(makePage(T("un.done.title"), T("un.done.subtitle", name), m_doneLabel));
    rv->addWidget(m_pages, 1);

    auto *buttons = new QHBoxLayout;
    m_cancelBtn = new QPushButton(T("btn.cancel"));
    m_nextBtn = new QPushButton;
    m_nextBtn->setObjectName("primary");
    m_nextBtn->setDefault(true);
    buttons->addWidget(m_cancelBtn);
    buttons->addStretch();
    buttons->addWidget(m_nextBtn);
    rv->addLayout(buttons);
    setWindowFlag(Qt::FramelessWindowHint, true);
    auto *rightWrap = new QWidget;
    auto *rw = new QVBoxLayout(rightWrap);
    rw->setContentsMargins(0, 0, 0, 0);
    rw->setSpacing(0);
    auto *titleBar = new TitleBar(this, TitleBar::Info | TitleBar::Minimize | TitleBar::Close);
    titleBar->setInfoToolTip(T("about.tooltip"));
    rw->addWidget(titleBar);
    rw->addWidget(right, 1);
    Chrome::enableDrag(side, this);
    connect(titleBar, &TitleBar::infoClicked, this, [this] { Chrome::showAbout(this, &T); });
    root->addWidget(rightWrap, 1);
    setCentralWidget(central);

    connect(m_nextBtn, &QPushButton::clicked, this, &UninstallerWindow::onNext);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QWidget::close);   // « Non » : on quitte sans rien supprimer
    goTo(Confirm);
}

QWidget *UninstallerWindow::makePage(const QString &title, const QString &subtitle, QWidget *body)
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    auto *t = new QLabel(title);
    t->setObjectName("pageTitle");
    t->setWordWrap(true);
    auto *s = new QLabel(subtitle);
    s->setObjectName("pageSub");
    s->setWordWrap(true);
    v->addWidget(t);
    v->addWidget(s);
    v->addSpacing(10);
    v->addWidget(body, 1);
    return w;
}

void UninstallerWindow::goTo(int page)
{
    m_cur = page;
    m_pages->setCurrentIndex(page);
    updateUi();
}

void UninstallerWindow::updateUi()
{
    static const char *keys[] = {"un.step.confirm", "un.step.progress", "un.step.done"};
    for (int i = 0; i < PageCount; ++i) {
        QLabel *l = m_stepLabels[i];
        l->setText(i == m_cur ? QString("%1  %2").arg(i + 1).arg(T(keys[i])) : QString::number(i + 1));
        l->setProperty("step", i == m_cur ? "active" : (i < m_cur ? "done" : "todo"));
        l->style()->unpolish(l);
        l->style()->polish(l);
    }
    m_cancelBtn->setVisible(m_cur == Confirm);
    m_nextBtn->setEnabled(m_cur != Progress);
    m_nextBtn->setText(T(m_cur == Done ? "btn.finish" : "un.btn.uninstall"));
}

void UninstallerWindow::onNext()
{
    if (m_cur == Confirm) { run(); return; }
    if (m_cur == Done) close();
}

void UninstallerWindow::closeEvent(QCloseEvent *e)
{
    if (m_busy) { e->ignore(); return; }
    e->accept();
}

void UninstallerWindow::run()
{
    m_busy = true;
    goTo(Progress);
    int errors = 0;
    const int n = qMax(1, int(m_files.size()));
    for (int i = 0; i < m_files.size(); ++i) {
        const QString &p = m_files[i];
        m_status->setText(T("un.removing", QFileInfo(p).fileName()));
        QApplication::processEvents();
        if (QFile::remove(p)) m_log->appendPlainText("- " + p);
        else { ++errors; m_log->appendPlainText("[ERROR] " + p); qWarning().noquote() << "[uninstall] cannot remove" << p; }
        m_bar->setValue((i + 1) * 100 / n);
        QApplication::processEvents();
    }
    // Dossiers : seulement s'ils sont vides (les fichiers de l'utilisateur ne sont jamais touchés)
    QDir().rmdir(m_dir + "/lang");
    QDir().rmdir(m_dir);
    QProcess::startDetached("update-desktop-database",
        QStringList{QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/applications"});

    QString text = T("un.done.text");
    if (errors) text += T("un.errors").replace("%1", QString::number(errors));
    m_doneLabel->setText(text);
    m_busy = false;
    goTo(Done);
}
