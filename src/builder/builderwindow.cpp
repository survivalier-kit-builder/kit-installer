#include "builderwindow.h"
#include "chrome.h"
#include "diagnostics.h"
#include "gpgsign.h"
#include "languages.h"
#include "packager.h"
#include "project.h"

extern "C" {
#include "kit_core.h"
}
#include "sidepanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QCloseEvent>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QProgressBar>
#include <QRegularExpression>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>

BuilderWindow::BuilderWindow()
{
    setWindowTitle(tr("Kit Builder — créateur d’installeurs en Kit"));
    resize(1000, 720);

    Diag::phase("BuilderWindow : panneau gauche");
    auto *central = new QWidget;
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Gauche : aperçu du panneau
    auto *left = new QWidget;
    left->setObjectName("leftPane");
    auto *lv = new QVBoxLayout(left);
    lv->setContentsMargins(20, 20, 20, 20);
    auto *cap = new QLabel(tr("Aperçu de l’installeur"));
    cap->setObjectName("previewCaption");
    m_panel = new SidePanel;
    lv->addWidget(cap);
    lv->addWidget(m_panel);
    lv->addStretch(1);
    root->addWidget(left);

    Diag::phase("BuilderWindow : formulaire");
    // Droite : formulaire
    auto *right = new QWidget;
    right->setObjectName("rightPane");
    auto *rv = new QVBoxLayout(right);
    rv->setContentsMargins(28, 24, 28, 20);
    rv->setSpacing(12);

    auto *title = new QLabel(tr("Créer un installeur en Kit"));
    title->setObjectName("pageTitle");
    auto *sub = new QLabel(tr("Renseignez le programme à empaqueter : un installeur .AppImage sera généré."));
    sub->setObjectName("pageSub");
    sub->setWordWrap(true);
    auto *head = new QHBoxLayout;
    auto *logo = new QLabel;
    logo->setPixmap(QPixmap(":/logo.png").scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto *titles = new QVBoxLayout;
    titles->setSpacing(4);
    titles->addWidget(title);
    titles->addWidget(sub);
    head->setSpacing(14);
    head->addWidget(logo, 0, Qt::AlignTop);
    head->addLayout(titles, 1);
    rv->addLayout(head);

    // Projet .kit-gen : toutes les informations pour regénérer l'installeur sans ressaisie
    auto *tools = new QHBoxLayout;
    tools->setSpacing(8);
    auto mkTool = [&](const QString &text) { auto *b = new QPushButton(text); b->setObjectName("tool"); tools->addWidget(b); return b; };
    QPushButton *bNew = mkTool(tr("Nouveau"));
    QPushButton *bOpen = mkTool(tr("Ouvrir…"));
    QPushButton *bSave = mkTool(tr("Enregistrer"));
    QPushButton *bSaveAs = mkTool(tr("Enregistrer sous…"));
    m_projLabel = new QLabel;
    m_projLabel->setObjectName("hint");
    m_projLabel->setMinimumWidth(10);
    tools->addSpacing(6);
    tools->addWidget(m_projLabel, 1);
    rv->addLayout(tools);
    connect(bNew, &QPushButton::clicked, this, &BuilderWindow::newProject);
    connect(bOpen, &QPushButton::clicked, this, &BuilderWindow::openDialog);
    connect(bSave, &QPushButton::clicked, this, &BuilderWindow::saveProject);
    connect(bSaveAs, &QPushButton::clicked, this, &BuilderWindow::saveProjectAs);
    connect(new QShortcut(QKeySequence::New, this), &QShortcut::activated, this, &BuilderWindow::newProject);
    connect(new QShortcut(QKeySequence::Open, this), &QShortcut::activated, this, &BuilderWindow::openDialog);
    connect(new QShortcut(QKeySequence::Save, this), &QShortcut::activated, this, &BuilderWindow::saveProject);
    connect(new QShortcut(QKeySequence::SaveAs, this), &QShortcut::activated, this, &BuilderWindow::saveProjectAs);

    auto *form = new QFormLayout;
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight);

    m_exe = new QLineEdit;  m_exe->setPlaceholderText(tr("Exécutable du programme à installer"));
    m_icon = new QLineEdit; m_icon->setPlaceholderText(tr("Icône (PNG, SVG, ICO…)"));
    m_name = new QLineEdit; m_version = new QLineEdit("1.0.0");
    m_dev = new QLineEdit;  m_email = new QLineEdit; m_site = new QLineEdit;
    m_licFile = new QLineEdit; m_licFile->setPlaceholderText(tr("Fichier de licence (optionnel)"));
    m_out = new QLineEdit(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));

    m_license = new QComboBox;
    m_license->setEditable(true);
    m_license->addItems({"MIT", "GPL-3.0", "GPL-2.0", "LGPL-3.0", "Apache-2.0", "BSD-3-Clause", "MPL-2.0",
                         QString::fromUtf8("Propriétaire")});

    m_category = new QComboBox;
    const QList<QPair<QString, QString>> cats = {
        {tr("Utilitaires"), "Utility"}, {tr("Développement"), "Development"}, {tr("Jeux"), "Game"},
        {tr("Graphisme"), "Graphics"}, {tr("Audio / Vidéo"), "AudioVideo"}, {tr("Internet"), "Network"},
        {tr("Bureautique"), "Office"}, {tr("Éducation"), "Education"}, {tr("Système"), "System"}};
    for (const auto &c : cats) m_category->addItem(c.first, c.second);

    m_desc = new QPlainTextEdit;
    m_desc->setPlaceholderText(tr("Description du programme (affichée à l’étape de bienvenue)"));
    m_desc->setFixedHeight(84);

    form->addRow(tr("Exécutable *"), rowWithButton(m_exe, tr("Choisir…"), SLOT(pickExe())));
    form->addRow(tr("Icône *"), rowWithButton(m_icon, tr("Choisir…"), SLOT(pickIcon())));
    form->addRow(tr("Nom *"), m_name);
    form->addRow(tr("Version"), m_version);
    form->addRow(tr("Description"), m_desc);
    form->addRow(tr("Développeur"), m_dev);
    form->addRow(tr("E-mail"), m_email);
    form->addRow(tr("Site web"), m_site);
    form->addRow(tr("Licence"), m_license);
    form->addRow(tr("Texte licence"), rowWithButton(m_licFile, tr("Choisir…"), SLOT(pickLicense())));
    form->addRow(tr("Catégorie"), m_category);

    // Langues prises en charge par le PROGRAMME : le développeur saisit les codes (en, fr, de...)
    Lang::ensureUserTemplate();
    auto *langBox = new QWidget;
    auto *lv2 = new QVBoxLayout(langBox);
    lv2->setContentsMargins(0, 0, 0, 0);
    lv2->setSpacing(6);
    m_progLangs = new QLineEdit;
    m_progLangs->setPlaceholderText(tr("ex. : en, fr, de"));
    QStringList names;
    for (const LangInfo &li : Lang::discover()) names << li.name;
    auto *langHint = new QLabel(tr("Codes des langues de votre programme, séparés par des virgules. L’installeur gère "
                                   "lui-même ses langues (%1) : vous n’avez rien à traduire.").arg(names.join(", ")));
    langHint->setObjectName("hint");
    langHint->setWordWrap(true);
    lv2->addWidget(m_progLangs);
    lv2->addWidget(langHint);
    form->addRow(tr("Langues du programme"), langBox);
    form->addRow(tr("Dossier de sortie"), rowWithButton(m_out, tr("Choisir…"), SLOT(pickOutput())));

    // Signature GPG de l'installeur généré
    auto *gpgBox = new QWidget;
    auto *gv = new QVBoxLayout(gpgBox);
    gv->setContentsMargins(0, 0, 0, 0);
    gv->setSpacing(6);
    m_sign = new QCheckBox(tr("Signer l’installeur avec une clé GPG"));
    auto *keyRow = new QHBoxLayout;
    m_gpgKey = new QComboBox;
    auto *refresh = new QPushButton(tr("Actualiser"));
    keyRow->addWidget(m_gpgKey, 1);
    keyRow->addWidget(refresh);
    m_gpgHint = new QLabel;
    m_gpgHint->setObjectName("hint");
    m_gpgHint->setWordWrap(true);
    gv->addWidget(m_sign);
    gv->addLayout(keyRow);
    gv->addWidget(m_gpgHint);
    form->addRow(tr("Signature GPG"), gpgBox);
    connect(m_sign, &QCheckBox::toggled, this, &BuilderWindow::updateGpgUi);
    connect(refresh, &QPushButton::clicked, this, &BuilderWindow::refreshGpgKeys);
    refreshGpgKeys();

    auto *formWidget = new QWidget;
    formWidget->setLayout(form);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(formWidget);
    rv->addWidget(scroll, 1);

    m_bar = new QProgressBar;
    m_bar->setRange(0, 100);
    m_bar->setTextVisible(false);
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setFixedHeight(100);
    m_buildBtn = new QPushButton(tr("Générer l’installeur .AppImage"));
    m_buildBtn->setObjectName("primary");
    rv->addWidget(m_bar);
    rv->addWidget(m_log);
    rv->addWidget(m_buildBtn);

    Diag::phase("BuilderWindow : assemblage");
    root->addWidget(right, 1);
    setCentralWidget(central);
    TitleBar *bar = Chrome::install(this, true);              // barre de titre personnalisée avec bouton « i »
    bar->setInfoToolTip(tr("À propos"));
    connect(bar, &TitleBar::infoClicked, this, [this] { Chrome::showAbout(this, nullptr); });

    connect(m_buildBtn, &QPushButton::clicked, this, &BuilderWindow::build);

    for (QLineEdit *e : {m_icon, m_name, m_version, m_dev})
        connect(e, &QLineEdit::textChanged, this, &BuilderWindow::updatePreview);
    for (QLineEdit *e : {m_exe, m_icon, m_name, m_version, m_dev, m_email, m_site, m_licFile, m_out, m_progLangs})
        connect(e, &QLineEdit::textChanged, this, [this] { markDirty(); });
    connect(m_desc, &QPlainTextEdit::textChanged, this, [this] { markDirty(); });
    connect(m_license, &QComboBox::currentTextChanged, this, [this] { markDirty(); });
    connect(m_category, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { markDirty(); });
    connect(m_gpgKey, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { markDirty(); });
    connect(m_sign, &QCheckBox::toggled, this, [this] { markDirty(); });
    Diag::phase("BuilderWindow : aperçu");
    updateTitle();
    updatePreview();
    Diag::phase("BuilderWindow : prêt");
}

QWidget *BuilderWindow::rowWithButton(QLineEdit *edit, const QString &text, const char *slot)
{
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto *b = new QPushButton(text);
    h->addWidget(edit, 1);
    h->addWidget(b);
    connect(b, SIGNAL(clicked()), this, slot);
    return w;
}

void BuilderWindow::log(const QString &line)
{
    if (line.isEmpty()) return;
    m_log->appendPlainText(line);
    qInfo().noquote() << "[build]" << line;
}

void BuilderWindow::setBusy(bool busy)
{
    m_buildBtn->setEnabled(!busy);
    if (busy) m_bar->setValue(0);
}

void BuilderWindow::pickExe()
{
    const QString f = QFileDialog::getOpenFileName(this, tr("Exécutable du programme"), QDir::homePath());
    if (f.isEmpty()) return;
    m_exe->setText(f);
    if (m_name->text().isEmpty()) m_name->setText(QFileInfo(f).completeBaseName());
}

void BuilderWindow::pickIcon()
{
    const QString f = QFileDialog::getOpenFileName(this, tr("Icône"), QDir::homePath(),
                                                   tr("Images (*.png *.svg *.jpg *.jpeg *.ico *.bmp *.webp)"));
    if (!f.isEmpty()) m_icon->setText(f);
}

void BuilderWindow::pickLicense()
{
    const QString f = QFileDialog::getOpenFileName(this, tr("Fichier de licence"), QDir::homePath(),
                                                   tr("Texte (*.txt *.md LICENSE* COPYING*);;Tous les fichiers (*)"));
    if (!f.isEmpty()) m_licFile->setText(f);
}

void BuilderWindow::pickOutput()
{
    const QString d = QFileDialog::getExistingDirectory(this, tr("Dossier de sortie"), m_out->text());
    if (!d.isEmpty()) m_out->setText(d);
}

void BuilderWindow::refreshGpgKeys()
{
    m_gpgKey->clear();
    if (Gpg::executable().isEmpty()) {
        m_sign->setChecked(false);
        m_sign->setEnabled(false);
        m_gpgHint->setText(tr("GnuPG (gpg) est introuvable : installez-le pour pouvoir signer (sudo apt install gnupg)."));
    } else {
        m_sign->setEnabled(true);
        for (const GpgKey &k : Gpg::secretKeys()) m_gpgKey->addItem(k.label(), k.fingerprint);
        m_gpgHint->setText(m_gpgKey->count() == 0
            ? tr("Aucune clé secrète trouvée. Créez-en une avec : gpg --full-generate-key, puis cliquez sur « Actualiser ».")
            : tr("Produit <installeur>.AppImage.asc (signature détachée). La clé publique est embarquée dans l’installeur, qui peut en afficher les informations (aucun autre fichier n’est créé). La phrase secrète éventuelle est demandée par gpg-agent."));
    }
    updateGpgUi();
}

void BuilderWindow::updateGpgUi()
{
    const bool on = m_sign->isChecked() && m_sign->isEnabled();
    m_gpgKey->setEnabled(on && m_gpgKey->count() > 0);
}

void BuilderWindow::updatePreview()
{
    m_panel->setLogo(QPixmap(m_icon->text()));
    m_panel->setProgramName(m_name->text());
    m_panel->setVersion(m_version->text().isEmpty() ? QString() : "v" + m_version->text());
    m_panel->setFooter(QString("© %1 | %2").arg(QDate::currentDate().year())
                           .arg(m_dev->text().isEmpty() ? m_name->text() : m_dev->text()));
}

/* ---------- Génération (en tâche de fond, sans outil externe) ---------- */

Project BuilderWindow::currentProject() const
{
    Project p;
    PackRequest &r = p.req;
    r.exe = m_exe->text();
    r.icon = m_icon->text();
    r.name = m_name->text();
    r.version = m_version->text();
    r.description = m_desc->toPlainText();
    r.developer = m_dev->text();
    r.email = m_email->text();
    r.website = m_site->text();
    r.license = m_license->currentText();
    r.licenseFile = m_licFile->text();
    r.category = m_category->currentData().toString();
    r.outDir = m_out->text();
    r.programLanguages = m_progLangs->text().split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
    p.sign = m_sign->isChecked();
    p.gpgKey = m_gpgKey->currentData().toString();
    r.gpgKey = p.sign ? p.gpgKey : QString();
    return p;
}

void BuilderWindow::build()
{
    const Project proj = currentProject();
    PackRequest r = proj.req;
    if (proj.sign && m_gpgKey->count() == 0) {
        QMessageBox::warning(this, tr("Signature GPG"), tr("Aucune clé GPG disponible : décochez la signature ou créez une clé."));
        return;
    }

    m_log->clear();
    setBusy(true);

    auto *pk = new Packager;
    connect(pk, &Packager::message, this, &BuilderWindow::log);
    connect(pk, &Packager::progress, m_bar, &QProgressBar::setValue);

    QThread *t = QThread::create([this, pk, r] {
        QString file, err;
        const bool ok = pk->run(r, &file, &err);
        QMetaObject::invokeMethod(this, [this, pk, ok, file, err] {
            pk->deleteLater();
            onDone(ok, file, err);
        }, Qt::QueuedConnection);
    });
    connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

void BuilderWindow::onDone(bool ok, const QString &file, const QString &error)
{
    setBusy(false);
    if (!ok) {
        log("[ERREUR] " + error);
        Diag::showError(this, tr("Génération impossible"), error);
        return;
    }
    const QString signedNote = QFileInfo::exists(file + ".asc") ? tr("\nSignature GPG : %1").arg(file + ".asc") : QString();
    if (QMessageBox::information(this, tr("Terminé"), tr("Installeur généré :\n%1%2").arg(file, signedNote),
                                 QMessageBox::Open | QMessageBox::Close) == QMessageBox::Open)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file).absolutePath()));
}

/* ---------- Projets .kit-gen ---------- */

void BuilderWindow::markDirty()
{
    if (m_loading || m_dirty) return;
    m_dirty = true;
    updateTitle();
}

void BuilderWindow::updateTitle()
{
    if (m_projectPath.isEmpty()) {
        setWindowTitle(tr("Kit Builder — créateur d’installeurs en Kit"));
        m_projLabel->setText(tr("Aucun projet (.kit-gen)"));
        m_projLabel->setToolTip(QString());
    } else {
        setWindowTitle(QString("Kit Builder — %1%2").arg(QFileInfo(m_projectPath).fileName(), m_dirty ? " *" : ""));
        m_projLabel->setText(QFileInfo(m_projectPath).fileName() + (m_dirty ? " *" : ""));
        m_projLabel->setToolTip(m_projectPath);
    }
}

bool BuilderWindow::confirmDiscard(const QString &title)
{
    if (!m_dirty || m_projectPath.isEmpty()) return true;
    return Chrome::confirm(this, title, tr("Le projet contient des modifications non enregistrées. Continuer et les perdre ?"),
                           tr("Oui"), tr("Non"));
}

void BuilderWindow::closeEvent(QCloseEvent *e)
{
    if (confirmDiscard(tr("Fermer Kit Builder"))) e->accept();
    else e->ignore();
}

void BuilderWindow::newProject()
{
    if (!confirmDiscard(tr("Nouveau projet"))) return;
    Project p;
    p.req.outDir = m_out->text();
    p.req.programLanguages.clear();
    applyProject(p, QString());
}

void BuilderWindow::openDialog()
{
    if (!confirmDiscard(tr("Ouvrir un projet"))) return;
    const QString start = m_projectPath.isEmpty() ? QDir::homePath() : QFileInfo(m_projectPath).absolutePath();
    const QString f = QFileDialog::getOpenFileName(this, tr("Ouvrir un projet Kit"), start,
                                                   tr("Projet Kit (*.kit-gen);;Tous les fichiers (*)"));
    if (!f.isEmpty()) openProject(f);
}

bool BuilderWindow::openProject(const QString &path)
{
    Project p;
    QString err;
    if (!ProjectFile::load(path, &p, &err)) {
        Diag::showError(this, tr("Ouverture impossible"), err);
        return false;
    }
    applyProject(p, QFileInfo(path).absoluteFilePath());
    log(tr("Projet ouvert : %1").arg(path));
    return true;
}

void BuilderWindow::applyProject(const Project &p, const QString &path)
{
    m_loading = true;
    const PackRequest &r = p.req;
    m_exe->setText(r.exe);
    m_icon->setText(r.icon);
    m_name->setText(r.name);
    m_version->setText(r.version.isEmpty() ? "1.0.0" : r.version);
    m_desc->setPlainText(r.description);
    m_dev->setText(r.developer);
    m_email->setText(r.email);
    m_site->setText(r.website);
    m_license->setCurrentText(r.license.isEmpty() ? "MIT" : r.license);
    m_licFile->setText(r.licenseFile);
    const int ci = m_category->findData(r.category);
    m_category->setCurrentIndex(ci >= 0 ? ci : 0);
    if (!r.outDir.isEmpty()) m_out->setText(r.outDir);
    m_progLangs->setText(r.programLanguages.join(", "));

    // Signature : on relit le trousseau et on retrouve la clé par son empreinte (ou son identifiant)
    refreshGpgKeys();
    int ki = p.gpgKey.isEmpty() ? -1 : m_gpgKey->findData(p.gpgKey);
    for (int i = 0; ki < 0 && !p.gpgKey.isEmpty() && i < m_gpgKey->count(); ++i) {
        const QString fp = m_gpgKey->itemData(i).toString();
        if (fp.endsWith(p.gpgKey, Qt::CaseInsensitive) || p.gpgKey.endsWith(fp, Qt::CaseInsensitive)) ki = i;
    }
    if (ki < 0 && !p.gpgKey.isEmpty()) {
        m_gpgKey->addItem(tr("(absente de ce trousseau)  %1").arg(p.gpgKey), p.gpgKey);
        ki = m_gpgKey->count() - 1;
        log(tr("[Attention] La clé GPG %1 du projet est absente de ce trousseau.").arg(p.gpgKey));
    }
    if (ki >= 0) m_gpgKey->setCurrentIndex(ki);
    m_sign->setChecked(p.sign && m_sign->isEnabled());
    updateGpgUi();

    m_projectPath = path;
    m_dirty = false;
    m_loading = false;
    updateTitle();
    updatePreview();
}

bool BuilderWindow::doSave(const QString &path)
{
    QString err;
    if (!ProjectFile::save(path, currentProject(), &err)) {
        Diag::showError(this, tr("Enregistrement impossible"), err);
        return false;
    }
    m_projectPath = path;
    m_dirty = false;
    updateTitle();
    log(tr("Projet enregistré : %1").arg(path));
    return true;
}

bool BuilderWindow::saveProject()
{
    return m_projectPath.isEmpty() ? saveProjectAs() : doSave(m_projectPath);
}

bool BuilderWindow::saveProjectAs()
{
    char id[128];
    kit_make_id(m_name->text().toUtf8().constData(), id, sizeof id);
    const QString dir = m_projectPath.isEmpty() ? QDir::homePath() : QFileInfo(m_projectPath).absolutePath();
    QString f = QFileDialog::getSaveFileName(this, tr("Enregistrer le projet Kit"),
                                             dir + "/" + QString::fromUtf8(id) + "." + ProjectFile::kSuffix,
                                             tr("Projet Kit (*.kit-gen)"));
    if (f.isEmpty()) return false;
    if (!f.endsWith("." + QString(ProjectFile::kSuffix), Qt::CaseInsensitive)) f += "." + QString(ProjectFile::kSuffix);
    return doSave(f);
}
