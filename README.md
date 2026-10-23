# Kit Installer

- Site officiel : https://survivalier.fast-page.org/kit-builder/
- Dépôt ForgeHub : https://forgehub.fast-page.org/survivalier/kit-builder

Générateur d'installeurs « en Kit » pour Linux — **aucune dépendance externe** pour produire les AppImage
(pas de linuxdeploy, appimagetool, mksquashfs ni FUSE à la génération).

## Compilation (depuis le dépôt GitHub)
    sudo apt install build-essential cmake qt6-base-dev     # zlib est déjà tiré par Qt
    cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
Le runtime AppImage (AppImage/type2-runtime, licence MIT) est téléchargé une fois au `cmake` s'il est absent
de `resources/runtime`, puis intégré au binaire `kit-builder`.

## Utilisation
Icône de l'application : `resources/logo.png` (fenêtres, en-tête du builder, lanceur). Pour ajouter Kit Builder au menu :
`./packaging/install-menu.sh` (ou `cmake --install build`).
Interface : `./build/kit-builder`
Console / CI (ex. GitHub Actions) :
    ./build/kit-builder --cli --exe ./monprog --icon icon.png --name "Mon Prog" \
        --version 1.0.0 --developer Moi --license MIT --out dist/
`kit-installer` doit rester à côté de `kit-builder`.

## Comment ça marche
`src/core/kit_appimage.c` écrit lui-même l'image SquashFS 4 (blocs compressés en zlib) et la colle au runtime ;
`src/builder/packager.cpp` embarque Qt (bibliothèques via `ldd` + plugins) comme le ferait linuxdeploy-plugin-qt.
Limites : fichiers < 4 Go, dossiers < ~65 Ko de listing ; l'AppImage résultante demande FUSE sur la machine
cible (standard AppImage) ou `APPIMAGE_EXTRACT_AND_RUN=1`.

## Diagnostic et rapports de plantage
- Les messages s'affichent dans le terminal **et** dans `~/.cache/kit-installer/<programme>.log`.
- `--debug` (ou `KIT_DEBUG=1`) : étapes de démarrage, plateforme/thème Qt, logs `qt.qpa.*`.
- Si le programme plante (segfault, abort, exception) **ou est arrêté de force** (kill -9, mémoire…), un processus
  surveillant ouvre une fenêtre avec le rapport (signal, dernière étape atteinte, pile d'appels, journal) et un bouton
  « Copier le rapport ». Une fermeture normale, Ctrl+C ou `kill` simple n'ouvre rien.
- Les erreurs gérées (génération impossible, échec d'installation, `qCritical`, exception C++) ouvrent la même fenêtre.
- Désactivation : `KIT_NO_WATCHER=1` (surveillant), `KIT_NO_CRASH_HANDLER=1` (gestionnaire de signaux).
- Pour isoler un problème d'affichage : `QT_QPA_PLATFORM=xcb ./kit-builder --debug`, puis `QT_QPA_PLATFORM=wayland ...`,
  puis `QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=Fusion ./kit-builder --debug`.

## Langues
**L'installeur gère lui-même ses langues** : le développeur n'en choisit aucune et n'a rien à traduire.
- Toutes les langues de `languages/*.lang` (fournies : English — langue par défaut —, Français, Deutsch, Español, Italiano,
  Nederlands, Português et Русский) sont
  intégrées à l'installeur. La 1re étape de l'installeur est le choix de la langue (celle du système est présélectionnée) ;
  toute l'interface change en direct, y compris l'assistant de désinstallation.
- Dans kit-builder, le champ **« Langues du programme »** est une simple zone de texte où le développeur saisit les codes
  des langues que *son programme* prend en charge (`en, fr, de`...). Ils sont affichés sur la page de bienvenue de
  l'installeur (« Langues prises en charge : English, Français, Deutsch »). CLI : `--program-languages en,fr,de`.
- **Ajouter une langue à l'installeur** (modèle réutilisable) : copiez `languages/_template.lang` en `<code>.lang`
  (ex. `de.lang`), changez `code=` et `name=`, traduisez ce qui est à droite des `=`, puis
  - soit placez-le dans `languages/` et recompilez (il est alors intégré à `kit-installer`),
  - soit déposez-le dans le dossier `languages/` à côté de `kit-builder` ou dans `~/.local/share/kit-installer/languages` :
    kit-builder l'embarque automatiquement dans les installeurs qu'il génère, sans recompiler.
  Les clés absentes retombent sur l'anglais.

## Option « Ajouter au PATH »
À l'étape Options, l'installeur peut créer la commande `<id-du-programme>` (ex. `mon-prog`) : lien symbolique
`~/.local/bin/<id>` vers le programme installé. Si `~/.local/bin` n'est pas encore dans le `PATH`, un bloc idempotent est
ajouté à `~/.profile` (et `~/.bashrc` / `~/.zshrc` s'ils existent) ; ouvrez un nouveau terminal ensuite.
Un fichier existant du même nom n'est jamais écrasé. `uninstall.sh` supprime le lien (pas le bloc PATH, partagé).

## Désinstallation
L'installeur copie l'AppImage dans le dossier d'installation (`uninstall.AppImage`, ~40 Mo) et l'utilise comme assistant de
désinstallation (même style, même langue que l'installation). Il est lancé par :
- l'entrée de menu « Désinstaller <programme> » et l'action « Désinstaller » du menu contextuel du lanceur ;
- ou `./uninstall.AppImage --uninstall` dans le dossier d'installation.
L'assistant liste ce qui sera supprimé et demande confirmation (« Annuler » ne supprime rien). Il ne supprime que les
fichiers listés dans `uninstall.list` (jamais de dossier, jamais vos données) ; les dossiers ne sont retirés que s'ils sont vides.
`uninstall.sh` reste disponible comme secours en mode texte.

## Barre de titre et « À propos »
Les fenêtres utilisent une barre de titre personnalisée (Qt ne permet pas d'ajouter un bouton à la barre native) avec le
bouton « i » (icône dessinée en vectoriel, sans police : pas de dépendance à fontconfig), réduire, agrandir (builder) et
fermer. Le bouton « i » ouvre la fenêtre « À propos » : logo, version, auteur, site et crédits des composants utilisés.
Les crédits sont dans `Chrome::showAbout()` (`src/common/chrome.cpp`) ; les textes de l'installeur sont dans les fichiers `.lang`
(clés `about.*`). Le déplacement utilise le gestionnaire de fenêtres (X11 et Wayland).

## Signature GPG
Dans kit-builder, section « Signature GPG » : cochez « Signer l'installeur » et choisissez une clé secrète dans la liste
(« Actualiser » relit votre trousseau). Le builder produit `<installeur>.AppImage.asc` (signature détachée, ASCII armor),
vérifiée aussitôt après création. **Aucune clé n'est exportée dans un fichier** : la clé publique et ses informations sont
embarquées dans l'AppImage (`payload/signing-key.asc` et `signing-key.ini`).
Dans l'installeur, si une clé est embarquée, la page de bienvenue affiche le bouton « Afficher la clé GPG... » : fenêtre
avec identité, empreinte, identifiant, algorithme, dates, sous-clés, bloc de clé publique, et boutons « Copier l'empreinte »
et « Copier la clé publique ». La clé seule ne prouve pas l'authenticité (la fenêtre le rappelle) : vos utilisateurs vérifient
le fichier `.asc` et comparent l'empreinte avec celle que vous publiez :
    gpg --verify <installeur>.AppImage.asc <installeur>.AppImage
La phrase secrète éventuelle est demandée par gpg-agent (pinentry).
CLI / CI : `--sign-key EMPREINTE|ID|EMAIL [--gpg-passphrase-file FICHIER]`. Une clé introuvable est refusée avant toute génération.
Nécessite `gpg` (sudo apt install gnupg) ; sans lui, la case est désactivée et un message l'explique.

## Liens cliquables
Dans l'installeur, le site web, l'e-mail du développeur, les URL http(s) de la description et du texte de licence sont
cliquables (ouverture via xdg-open). Seuls les schémas http, https et mailto sont autorisés.

## Fermeture de l'installeur
Toute fermeture de l'installeur (croix de la barre de titre, bouton « Annuler », Alt+F4, gestionnaire de fenêtres) ouvre une
fenêtre de validation Oui / Non au style de l'interface (« Non » est le choix par défaut, la croix de la fenêtre vaut « Non »).
Pendant la copie des fichiers, « Oui » interrompt l'installation en cours et les fichiers déjà copiés sont retirés.
Sur la page finale, la fenêtre se ferme sans confirmation. Textes : clés `quit.*` des fichiers `.lang`.

## Projets .kit-gen
Un fichier `.kit-gen` (JSON lisible) contient toutes les informations pour générer un installeur sans ressaisie : exécutable,
icône, nom, version, description, développeur, e-mail, site, licence (+ fichier), catégorie, langues du programme, dossier de
sortie et signature GPG (empreinte de la clé uniquement, jamais de secret). Les chemins sont enregistrés **relatifs au fichier**
(projets déplaçables, dépôts GitHub) ; voir `examples/example.kit-gen`.
- Dans kit-builder : boutons **Nouveau / Ouvrir… / Enregistrer / Enregistrer sous…** (Ctrl+N / O / S / Maj+S). Le titre de la
  fenêtre indique le projet et `*` s'il a des modifications non enregistrées (confirmation avant de les perdre).
- En ligne de commande : `kit-builder projet.kit-gen` ouvre le projet ; `kit-builder --cli projet.kit-gen` génère l'AppImage
  sans interface (options `--version`, `--out`, `--sign-key`... pour surcharger, ex. dans une CI). Le dossier de sortie est créé s'il manque.
- **Double-clic** : `./packaging/install-menu.sh` (ou `cmake --install build` puis `update-mime-database /usr/share/mime`)
  enregistre le type MIME `application/x-kit-gen` (`packaging/kit-gen.xml`) et associe les `.kit-gen` à Kit Builder.

## Icône des fenêtres
Toutes les fenêtres (builder, installeur, désinstalleur, dialogues) affichent uniquement l'icône de Kit Builder ; l'icône du
programme installé reste dans le panneau de gauche et dans les entrées de menu créées pour ce programme.
