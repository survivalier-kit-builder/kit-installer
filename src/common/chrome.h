#pragma once
/* Barre de titre personnalisée (avec bouton « i ») et fenêtre « À propos ». */
#include <QDialog>
#include <QPoint>
#include <QWidget>

class QLabel;
class QMainWindow;
class QToolButton;
class Translator;

class TitleBar : public QWidget {
    Q_OBJECT
public:
    enum Button { Info = 1, Minimize = 2, Maximize = 4, Close = 8 };
    TitleBar(QWidget *window, int buttons, QWidget *parent = nullptr);
    void setInfoToolTip(const QString &t);

signals:
    void infoClicked();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    void refreshTitle();
    void refreshIcon();

    QWidget *m_win;
    QLabel *m_icon, *m_title;
    QToolButton *m_info = nullptr;
    bool m_canMax = false, m_manualDrag = false;
    QPoint m_dragOffset;
};

namespace Chrome {
constexpr int kBarHeight = 38;
/* Fenêtre entière : retire le cadre du système et ajoute la barre en haut. À appeler après setCentralWidget(). */
TitleBar *install(QMainWindow *w, bool resizable);
/* Rend un widget (ex. panneau latéral) capable de déplacer la fenêtre. */
void enableDrag(QWidget *source, QWidget *window);
/* Fenêtre de validation (Oui / Non) au style de l'interface. La croix de sa barre = « Non ». "Non" est le bouton par défaut. */
bool confirm(QWidget *parent, const QString &title, const QString &text, const QString &yesText, const QString &noText);
/* Fenêtre « À propos » : logo + crédits. t = traducteur de l'installeur (nullptr : textes français du builder). */
void showAbout(QWidget *parent, const Translator *t);
}
