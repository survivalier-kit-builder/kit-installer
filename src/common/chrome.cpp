#include "chrome.h"
#include "languages.h"

#include <QApplication>
#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSizeGrip>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#ifndef KIT_VERSION
#define KIT_VERSION "2.1.0"
#endif

/* ---------- Icônes dessinées (aucune police : pas de dépendance à fontconfig) ---------- */
enum Glyph { GInfo, GMin, GMax, GClose };

static QPixmap glyph(Glyph kind, const QColor &c)
{
    const qreal dpr = 2.0;
    QPixmap pm(int(16 * dpr), int(16 * dpr));
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(dpr);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    switch (kind) {
    case GInfo:                                   // cercle + « i » (équivalent du symbole circled information source)
        p.drawEllipse(QRectF(1.5, 1.5, 13, 13));
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(8, 5.0), 1.05, 1.05);
        p.setPen(pen);
        p.drawLine(QPointF(8, 7.4), QPointF(8, 11.6));
        break;
    case GMin:   p.drawLine(QPointF(3.5, 11.5), QPointF(12.5, 11.5)); break;
    case GMax:   p.drawRoundedRect(QRectF(3.5, 3.5, 9, 9), 1.2, 1.2); break;
    case GClose:
        p.drawLine(QPointF(4, 4), QPointF(12, 12));
        p.drawLine(QPointF(12, 4), QPointF(4, 12));
        break;
    }
    return pm;
}

static QToolButton *makeButton(const char *name, Glyph kind, QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setObjectName(name);
    b->setFocusPolicy(Qt::NoFocus);
    b->setAutoRaise(true);
    b->setFixedSize(42, Chrome::kBarHeight);
    QIcon ic;
    ic.addPixmap(glyph(kind, QColor("#4a5a8a")), QIcon::Normal);
    ic.addPixmap(glyph(kind, kind == GClose ? QColor(Qt::white) : QColor("#3d5bd9")), QIcon::Active);
    b->setIcon(ic);
    b->setIconSize(QSize(16, 16));
    return b;
}

/* ---------- Barre de titre ---------- */
TitleBar::TitleBar(QWidget *window, int buttons, QWidget *parent)
    : QWidget(parent), m_win(window), m_canMax(buttons & Maximize)
{
    setObjectName("titleBar");
    setAttribute(Qt::WA_StyledBackground);
    setFixedHeight(Chrome::kBarHeight);

    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(12, 0, 0, 0);
    h->setSpacing(8);
    m_icon = new QLabel;
    m_icon->setFixedSize(20, 20);
    m_icon->setScaledContents(true);
    m_icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_title = new QLabel;
    m_title->setObjectName("titleText");
    m_title->setAttribute(Qt::WA_TransparentForMouseEvents);
    h->addWidget(m_icon);
    h->addWidget(m_title, 1);

    if (buttons & Info) {
        m_info = makeButton("winBtn", GInfo, this);
        h->addWidget(m_info);
        connect(m_info, &QToolButton::clicked, this, &TitleBar::infoClicked);
    }
    if (buttons & Minimize) {
        auto *b = makeButton("winBtn", GMin, this);
        h->addWidget(b);
        connect(b, &QToolButton::clicked, m_win, &QWidget::showMinimized);
    }
    if (buttons & Maximize) {
        auto *b = makeButton("winBtn", GMax, this);
        h->addWidget(b);
        connect(b, &QToolButton::clicked, this, [this] { m_win->isMaximized() ? m_win->showNormal() : m_win->showMaximized(); });
    }
    if (buttons & Close) {
        auto *b = makeButton("winClose", GClose, this);
        h->addWidget(b);
        connect(b, &QToolButton::clicked, m_win, &QWidget::close);
    }

    connect(m_win, &QWidget::windowTitleChanged, this, [this] { refreshTitle(); });
    connect(m_win, &QWidget::windowIconChanged, this, [this] { refreshIcon(); });
    refreshTitle();
    refreshIcon();
}

void TitleBar::setInfoToolTip(const QString &t) { if (m_info) m_info->setToolTip(t); }
void TitleBar::refreshTitle() { m_title->setText(m_win->windowTitle()); }
void TitleBar::refreshIcon() { m_icon->setPixmap(m_win->windowIcon().pixmap(40, 40)); }

void TitleBar::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    if (QWindow *wh = m_win->windowHandle())
        if (wh->startSystemMove()) return;                 // déplacement géré par le gestionnaire de fenêtres (X11 / Wayland)
    m_manualDrag = true;                                   // secours : déplacement manuel
    m_dragOffset = QCursor::pos() - m_win->frameGeometry().topLeft();
}

void TitleBar::mouseMoveEvent(QMouseEvent *e)
{
    if (m_manualDrag && (e->buttons() & Qt::LeftButton) && !m_win->isMaximized())
        m_win->move(QCursor::pos() - m_dragOffset);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *)
{
    if (m_canMax) m_win->isMaximized() ? m_win->showNormal() : m_win->showMaximized();
}

/* ---------- Installation sur une fenêtre ---------- */
namespace {
class GripPlacer : public QObject {
public:
    GripPlacer(QWidget *root, QSizeGrip *grip) : QObject(root), m_root(root), m_grip(grip) { root->installEventFilter(this); }
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (o == m_root && e->type() == QEvent::Resize) {
            m_grip->move(m_root->width() - m_grip->width() - 2, m_root->height() - m_grip->height() - 2);
            m_grip->raise();
        }
        return false;
    }
private:
    QWidget *m_root;
    QSizeGrip *m_grip;
};

class DragFilter : public QObject {
public:
    DragFilter(QWidget *source, QWidget *win) : QObject(source), m_win(win) { source->installEventFilter(this); }
    bool eventFilter(QObject *, QEvent *e) override
    {
        if (e->type() == QEvent::MouseButtonPress && static_cast<QMouseEvent *>(e)->button() == Qt::LeftButton)
            if (QWindow *wh = m_win->windowHandle())
                return wh->startSystemMove();
        return false;
    }
private:
    QWidget *m_win;
};
}  // namespace

TitleBar *Chrome::install(QMainWindow *w, bool resizable)
{
    w->setWindowFlag(Qt::FramelessWindowHint, true);
    QWidget *old = w->takeCentralWidget();
    auto *root = new QWidget;
    auto *v = new QVBoxLayout(root);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto *bar = new TitleBar(w, TitleBar::Info | TitleBar::Minimize | (resizable ? TitleBar::Maximize : 0) | TitleBar::Close);
    v->addWidget(bar);
    v->addWidget(old, 1);
    w->setCentralWidget(root);
    if (resizable) {
        auto *grip = new QSizeGrip(root);
        grip->setFixedSize(16, 16);
        new GripPlacer(root, grip);
    }
    return bar;
}

void Chrome::enableDrag(QWidget *source, QWidget *window) { new DragFilter(source, window); }

/* ---------- Validation ---------- */
bool Chrome::confirm(QWidget *parent, const QString &title, const QString &text, const QString &yesText, const QString &noText)
{
    QDialog dlg(parent);
    dlg.setObjectName("aboutDialog");
    dlg.setWindowFlag(Qt::FramelessWindowHint, true);
    dlg.setWindowTitle(title);
    dlg.setWindowIcon(parent ? parent->windowIcon() : QIcon(":/logo.png"));
    dlg.setFixedWidth(460);

    auto *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(new TitleBar(&dlg, TitleBar::Close));          // la croix = « Non »

    auto *body = new QWidget;
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(28, 22, 28, 22);
    v->setSpacing(22);
    auto *msg = new QLabel(text);
    msg->setWordWrap(true);
    QFont f = msg->font();
    f.setPointSizeF(f.pointSizeF() + 1);
    msg->setFont(f);
    v->addWidget(msg);

    auto *row = new QHBoxLayout;
    auto *yes = new QPushButton(yesText);
    auto *no = new QPushButton(noText);
    no->setObjectName("primary");                                   // choix sûr mis en avant et par défaut
    no->setDefault(true);
    row->addStretch(1);
    row->addWidget(yes);
    row->addWidget(no);
    v->addLayout(row);
    outer->addWidget(body);

    bool result = false;
    QObject::connect(yes, &QPushButton::clicked, &dlg, [&] { result = true; dlg.accept(); });
    QObject::connect(no, &QPushButton::clicked, &dlg, &QDialog::reject);
    dlg.adjustSize();
    if (parent) {                                                   // centrée sur la fenêtre parente
        const QPoint c = parent->mapToGlobal(parent->rect().center());
        dlg.move(c - QPoint(dlg.width() / 2, dlg.height() / 2));
    }
    no->setFocus();
    dlg.exec();
    return result;
}

/* ---------- À propos ---------- */
void Chrome::showAbout(QWidget *parent, const Translator *t)
{
    auto L = [&](const char *key, const QString &french, const QString &arg = QString()) {
        return t ? (*t)(key, arg) : QString(french).replace("%1", arg);
    };
    const QString author = "Survivalier";
    const QString site = "https://survivalier.fast-page.org";
    const QString appName = "Kit Installer";

    QDialog dlg(parent);
    dlg.setObjectName("aboutDialog");
    dlg.setWindowFlag(Qt::FramelessWindowHint, true);
    dlg.setWindowTitle(L("about.title", "À propos"));
    dlg.setWindowIcon(QIcon(":/logo.png"));
    dlg.setFixedSize(460, 590);

    auto *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto *bar = new TitleBar(&dlg, TitleBar::Close);
    outer->addWidget(bar);

    auto *body = new QWidget;
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(32, 18, 32, 24);
    v->setSpacing(6);
    auto center = [](QLabel *l) { l->setAlignment(Qt::AlignHCenter); l->setWordWrap(true); return l; };

    auto *logo = new QLabel;
    logo->setPixmap(QPixmap(":/logo.png").scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignHCenter);
    auto *name = center(new QLabel(appName));
    name->setObjectName("pageTitle");
    auto *ver = center(new QLabel(L("about.version", "Version %1", QString::fromLatin1(KIT_VERSION))));
    ver->setObjectName("pageSub");
    auto *tag = center(new QLabel(L("side.title", "Programme d’installation en Kit pour Linux")));
    tag->setObjectName("pageSub");

    auto *by = center(new QLabel(QString("<b>%1</b><br><a href='%2'>%2</a>")
                                     .arg(L("about.by", "Développé par %1", author).toHtmlEscaped(), site)));
    by->setTextFormat(Qt::RichText);
    by->setOpenExternalLinks(true);

    auto *builtTitle = center(new QLabel(QString("<b>%1</b>").arg(L("about.built_with", "Construit avec").toHtmlEscaped())));
    auto *built = center(new QLabel(QString("Qt %1  -  LGPL v3<br>AppImage type2-runtime  -  MIT<br>zlib  -  zlib License")
                                        .arg(QString::fromLatin1(qVersion()))));
    built->setTextFormat(Qt::RichText);
    built->setObjectName("hint");
    auto *copy = center(new QLabel(QString("© %1 | %2").arg(QDate::currentDate().year()).arg(author)));
    copy->setObjectName("hint");

    auto *close = new QPushButton(L("about.close", "Fermer"));
    close->setObjectName("primary");
    close->setDefault(true);
    QObject::connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);

    v->addWidget(logo);
    v->addWidget(name);
    v->addWidget(ver);
    v->addWidget(tag);
    v->addSpacing(14);
    v->addWidget(by);
    v->addSpacing(10);
    v->addWidget(builtTitle);
    v->addWidget(built);
    v->addStretch(1);
    v->addWidget(copy);
    v->addSpacing(6);
    v->addWidget(close, 0, Qt::AlignHCenter);
    outer->addWidget(body, 1);
    dlg.exec();
}
