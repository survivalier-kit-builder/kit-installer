#include "sidepanel.h"

#include <QLinearGradient>
#include <QPainter>

SidePanel::SidePanel(QWidget *parent)
    : QWidget(parent),
      m_title(QStringLiteral("Programme d’installation en Kit pour Linux"))
{
    setFixedSize(300, 600);
}

void SidePanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                     QPainter::SmoothPixmapTransform);

    // Fond dégradé (bas-gauche -> haut-droite)
    QLinearGradient g(0, 600, 300, 0);
    g.setColorAt(0.0, QColor("#6a85ff"));
    g.setColorAt(0.5, QColor("#8fc3ff"));
    g.setColorAt(1.0, QColor("#b2fbff"));
    p.fillRect(rect(), g);

    // Titre
    QFont f = font();
    f.setPixelSize(19);
    f.setBold(true);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(8, 28, 284, 64), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_title);

    // Cadre blanc translucide du logo : x=30, y=109, 90x90
    const QRect box(30, 109, 90, 90);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 150));
    p.drawRoundedRect(box, 20, 20);

    if (!m_logo.isNull()) {
        const QPixmap scaled = m_logo.scaled(66, 66, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        p.drawPixmap(box.center().x() - scaled.width() / 2 + 1,
                     box.center().y() - scaled.height() / 2 + 1, scaled);
    }

    // Nom + version à droite du cadre
    p.setPen(Qt::white);
    f.setPixelSize(17);
    p.setFont(f);
    p.drawText(QRect(134, 112, 142, 48), Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap, m_name);
    f.setPixelSize(12);
    f.setBold(false);
    p.setFont(f);
    p.setPen(QColor(255, 255, 255, 220));
    p.drawText(QRect(134, 164, 142, 24), Qt::AlignLeft | Qt::AlignTop, m_version);

    // Pied de page
    p.setPen(QPen(QColor(255, 255, 255, 200), 1));
    p.drawLine(30, 556, 270, 556);
    f.setPixelSize(14);
    f.setBold(true);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 562, 300, 28), Qt::AlignCenter, m_footer);
}
