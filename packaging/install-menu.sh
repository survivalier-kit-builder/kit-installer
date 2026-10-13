#!/bin/sh
# Ajoute Kit Builder au menu des applications et l'associe aux fichiers .kit-gen (double-clic), sans installation système.
# Usage : ./packaging/install-menu.sh [chemin/vers/kit-builder]   (défaut : ./build/kit-builder)
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$(readlink -f "${1:-$ROOT/build/kit-builder}")"
[ -x "$BIN" ] || { echo "kit-builder introuvable : $BIN (compilez d'abord)"; exit 1; }
DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
ICONDIR="$DATA/icons/hicolor/256x256/apps"
MIMEICONDIR="$DATA/icons/hicolor/256x256/mimetypes"
APPDIR="$DATA/applications"
MIMEDIR="$DATA/mime"
mkdir -p "$ICONDIR" "$MIMEICONDIR" "$APPDIR" "$MIMEDIR/packages"
cp "$ROOT/resources/logo.png" "$ICONDIR/kit-builder.png"
cp "$ROOT/resources/logo.png" "$MIMEICONDIR/application-x-kit-gen.png"
sed "s|^Exec=.*|Exec=\"$BIN\" %f|" "$ROOT/packaging/kit-builder.desktop" > "$APPDIR/kit-builder.desktop"
cp "$ROOT/packaging/kit-gen.xml" "$MIMEDIR/packages/kit-gen.xml"
update-mime-database "$MIMEDIR" >/dev/null 2>&1 || echo "(update-mime-database introuvable : installez shared-mime-info pour l'association .kit-gen)"
update-desktop-database "$APPDIR" >/dev/null 2>&1 || true
xdg-mime default kit-builder.desktop application/x-kit-gen >/dev/null 2>&1 || true
gtk-update-icon-cache -f -t "$DATA/icons/hicolor" >/dev/null 2>&1 || true
echo "Kit Builder ajouté au menu et associé aux fichiers .kit-gen."
