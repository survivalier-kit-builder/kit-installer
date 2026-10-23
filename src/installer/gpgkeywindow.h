#pragma once
#include <QString>

class QWidget;
class Translator;

namespace GpgKeyWindow {
/* true si l'installeur embarque une clé GPG (payload/signing-key.ini). */
bool available(const QString &payloadDir);
/* Ouvre la fenêtre d'informations de la clé GPG embarquée. */
void show(QWidget *parent, const QString &payloadDir, const Translator &T);
}
