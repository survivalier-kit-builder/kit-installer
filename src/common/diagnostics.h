#pragma once
/* Diagnostic : journal terminal + fichier, fenêtre d'erreur, rapport de plantage (SIGSEGV...). */
#include <QApplication>
#include <QString>

class QWidget;

namespace Diag {
/* À appeler au tout début de main(), AVANT QApplication. */
void install(const char *appName, int argc, char **argv);
/* À appeler juste après la création de QApplication. */
void appCreated();
/* Mémorise (et affiche en mode --debug) la dernière étape atteinte : utile pour localiser un plantage. */
void phase(const char *name);
bool debug();
QString logPath();
/* Fenêtre d'erreur avec détails sélectionnables + bouton « Copier ». */
void showError(QWidget *parent, const QString &title, const QString &text, const QString &details = QString());
/* À appeler avant de quitter main() : signale un arrêt normal (rc == 0) au processus surveillant. */
void finish(int rc);
/* Modes internes : processus surveillant (--watch) et fenêtre de rapport (--crash-report). */
bool watcherRequested(int argc, char **argv);
int runWatcher(int argc, char **argv);
/* Mode « fenêtre de rapport » lancé automatiquement après un plantage. */
bool crashReporterRequested(int argc, char **argv, QString *reportPath);
int runCrashReporter(int &argc, char **argv, const QString &reportPath);
}

/* QApplication qui attrape les exceptions C++ non gérées et les affiche. */
class DiagApplication : public QApplication {
public:
    using QApplication::QApplication;
    bool notify(QObject *receiver, QEvent *event) override;
};
