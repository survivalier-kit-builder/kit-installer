#pragma once
#include <QPixmap>
#include <QString>
#include <QWidget>

/* Panneau latéral 300x600 : dégradé, titre, cadre blanc pour le logo, pied de page. */
class SidePanel : public QWidget {
public:
    explicit SidePanel(QWidget *parent = nullptr);
    void setLogo(const QPixmap &logo)      { m_logo = logo; update(); }
    void setProgramName(const QString &n)  { m_name = n; update(); }
    void setVersion(const QString &v)      { m_version = v; update(); }
    void setFooter(const QString &f)       { m_footer = f; update(); }
    void setTitle(const QString &t)        { m_title = t; update(); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QPixmap m_logo;
    QString m_title, m_name, m_version, m_footer;
};
