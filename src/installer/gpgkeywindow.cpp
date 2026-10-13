#include "gpgkeywindow.h"
#include "chrome.h"
#include "languages.h"

#include <QClipboard>
#include <QDialog>
#include <QFile>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

namespace GpgKeyWindow {

bool available(const QString &dir) { return QFile::exists(dir + "/signing-key.ini"); }

static QString groupedFingerprint(const QString &fp)           // 7E71 BEE5 270F ...  (2 blocs de 5 groupes)
{
    QString out;
    for (int i = 0; i < fp.size(); i += 4) {
        if (i) out += (i == 20) ? "  " : " ";
        out += fp.mid(i, 4);
    }
    return out;
}

void show(QWidget *parent, const QString &dir, const Translator &T)
{
    QSettings ini(dir + "/signing-key.ini", QSettings::IniFormat);
    ini.beginGroup("key");
    const QString fp = ini.value("fingerprint").toString();
    const QString keyId = ini.value("keyid").toString();
    const QString algo = ini.value("algorithm").toString();
    const QString created = ini.value("created").toString();
    QString expires = ini.value("expires").toString();
    if (expires.isEmpty()) expires = T("gpg.never");
    QStringList uids, subs;
    for (int i = 0, n = ini.value("uid_count").toInt(); i < n; ++i) uids << ini.value(QString("uid_%1").arg(i)).toString();
    for (int i = 0, n = ini.value("sub_count").toInt(); i < n; ++i) {
        const QStringList p = ini.value(QString("sub_%1").arg(i)).toString().split('|');   // algo|id|créée|expire|capacités
        if (p.size() >= 5) subs << QString("%1  -  %2  -  %3%4").arg(p[0], p[1], p[2], p[3].isEmpty() ? QString() : "  ->  " + p[3]);
    }
    ini.endGroup();
    QFile kf(dir + "/signing-key.asc");
    const QString armored = kf.open(QIODevice::ReadOnly) ? QString::fromUtf8(kf.readAll()) : QString();

    QDialog dlg(parent);
    dlg.setObjectName("aboutDialog");
    dlg.setWindowFlag(Qt::FramelessWindowHint, true);
    dlg.setWindowTitle(T("gpg.title"));
    dlg.setWindowIcon(QIcon(":/logo.png"));
    dlg.resize(660, 700);

    auto *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(new TitleBar(&dlg, TitleBar::Close));

    auto *content = new QWidget;
    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(28, 16, 28, 20);
    v->setSpacing(10);

    auto *title = new QLabel(T("gpg.title"));
    title->setObjectName("pageTitle");
    v->addWidget(title);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(1, 1);
    int row = 0;
    auto add = [&](const char *key, const QString &value, bool mono = false) {
        if (value.isEmpty()) return;
        auto *k = new QLabel(T(key));
        k->setObjectName("hint");
        k->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        auto *val = new QLabel(value.toHtmlEscaped().replace("\n", "<br>"));
        val->setTextFormat(Qt::RichText);
        val->setWordWrap(true);
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        QFont f = val->font();
        f.setBold(true);
        if (mono) { f = QFontDatabase::systemFont(QFontDatabase::FixedFont); f.setBold(true); }
        val->setFont(f);
        grid->addWidget(k, row, 0);
        grid->addWidget(val, row, 1);
        ++row;
    };
    add("gpg.identity", uids.join("\n"));
    add("gpg.fingerprint", groupedFingerprint(fp), true);
    add("gpg.key_id", keyId, true);
    add("gpg.algorithm", algo);
    add("gpg.created", created);
    add("gpg.expires", expires);
    add("gpg.subkeys", subs.join("\n"));
    v->addLayout(grid);

    auto *keyTitle = new QLabel(T("gpg.public_key"));
    keyTitle->setObjectName("hint");
    auto *box = new QPlainTextEdit(armored);
    box->setReadOnly(true);
    box->setLineWrapMode(QPlainTextEdit::NoWrap);
    box->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    box->setMinimumHeight(130);
    auto *note = new QLabel(T("gpg.note"));
    note->setObjectName("hint");
    note->setWordWrap(true);
    v->addWidget(keyTitle);
    v->addWidget(box, 1);
    v->addWidget(note);

    auto *buttons = new QHBoxLayout;
    auto *copyFp = new QPushButton(T("gpg.copy_fp"));
    auto *copyKey = new QPushButton(T("gpg.copy_key"));
    auto *close = new QPushButton(T("about.close"));
    close->setObjectName("primary");
    buttons->addWidget(copyFp);
    buttons->addWidget(copyKey);
    buttons->addStretch(1);
    buttons->addWidget(close);
    v->addLayout(buttons);

    auto flash = [&T](QPushButton *b, const QString &original) {
        b->setText(T("gpg.copied"));
        QTimer::singleShot(1500, b, [b, original] { b->setText(original); });
    };
    QObject::connect(copyFp, &QPushButton::clicked, &dlg, [=] {
        QGuiApplication::clipboard()->setText(fp);
        flash(copyFp, T("gpg.copy_fp"));
    });
    QObject::connect(copyKey, &QPushButton::clicked, &dlg, [=] {
        QGuiApplication::clipboard()->setText(armored);
        flash(copyKey, T("gpg.copy_key"));
    });
    QObject::connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
    dlg.exec();
}

}  // namespace GpgKeyWindow
