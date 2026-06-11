# CLAUDE.md — Console de streaming portable (projet DIY)

> Ce fichier donne le contexte complet du projet à Claude Code. Lis-le en entier
> avant toute modification. Le repo est un **fork de `moonlight-stream/moonlight-qt`**.

---

## 1. Le but final

Construire une **console de jeu portable, peu coûteuse à produire**, qui ne fait *pas*
tourner les jeux en local : elle **streame les jeux depuis un PC** (le PC fait le rendu,
la console n'est qu'un client d'affichage + manette). L'objectif est de permettre de jouer
à des jeux gourmands (AAA) sur un appareil léger à bonne autonomie, dès lors qu'on a une
bonne connexion.

Deux horizons :
- **Court terme** : un prototype fonctionnel à base de cartes off-the-shelf pour valider
  l'expérience et le logiciel.
- **Long terme** : un **produit grand public**, compact, pas cher à fabriquer, et à terme
  utilisable en mobilité via un **module 5G** (la difficulté finale, volontairement
  repoussée à la fin).

### Le principe directeur, non négociable : ZÉRO FRICTION

L'expérience doit être **aussi proche que possible d'une vraie console**, pour un novice
comme pour un expert. Conséquences concrètes pour tout ce qu'on code :
- **Aucun menu Linux, aucune fenêtre, aucun bureau visible.** Jamais.
- Au démarrage, la console **boote directement** sur l'interface de jeux et se **connecte
  automatiquement** au PC host.
- L'appairage (PIN) doit être **invisible** pour l'utilisateur (pré-appairage à la config,
  ou auto-acceptation du certificat).
- Tout doit donner le ressenti « j'allume, je joue ».

---

## 2. État actuel du projet

- Le streaming est **validé depuis ~1 an** : Moonlight + Sunshine + Tailscale, qui
  fonctionne bien, y compris **en 5G depuis un smartphone** (le chemin réseau/NAT est donc
  gérable — le concept est prouvé).
- Le repo est un **fork de moonlight-qt** (officiel), développé sous **Fedora KDE Plasma**,
  en **Qt 6**.
- Le fork **compile et se lance**, et le **décodage matériel fonctionne** (GPU Intel Iris Xe,
  pilote iHD/VAAPI, décodage H.264/HEVC/**AV1** confirmé).
- Une **interface "Big Picture" maison** a été conçue et validée visuellement (voir §5).
  Les fichiers QML vivent dans `app/gui/console/` (6 fichiers à ce jour).
- ✅ **Intégration au build faite** (commit `64bb96a0` sur `console-ui`) : 3 coutures dans
  `app.pro`, `qml.qrc`, `main.cpp`, derrière `embedded`/`CONSOLE_UI`. Le binaire `embedded`
  démarre sur `ConsoleHome`, le binaire vanilla est inchangé.
- ✅ **UI branchée sur les vraies données Moonlight** (commits `e0f7600c` + `5df70d84`) :
  `ConsoleHome` lit `ComputerModel`/`AppModel`, sélectionne automatiquement le premier host
  online+paired, et le bouton Jouer crée une vraie session via `StreamSegue.qml`.
- ✅ **Appairage automatique côté console + refonte visuelle "pro"** : dès qu'un host en
  ligne non appairé est détecté, la console lance `pairComputer()` elle-même et affiche un
  **code de liaison** plein écran (`PairingOverlay.qml`, façon appairage d'app TV) ; succès
  → carrousel, échec → nouvelle tentative auto après 5 s. Corrigé au passage le bug
  "Recherche…" permanent (modèles initialisés après liaison aux vues, cf §11) et masqué la
  toolbar Material de `main.qml` depuis `ConsoleHome` (zéro élément bureau). Nouveaux
  fichiers : `Spinner.qml`, `PairingOverlay.qml`. Le direct-launch (§9 6a) est câblé.
- ✅ **Chaîne complète validée en réel contre Apollo** (host « Djinger », juin 2026) :
  découverte → code de liaison saisi une fois → carrousel avec les vraies apps → stream.
  Deux correctifs en route (commits `25c02abb` + `c23148af`) :
  - `AppModel`/`ComputerModel` n'exposent **pas** de `count` en QML (cf §11) → le carrousel
    ne s'affichait jamais (« Chargement… » infini) ; tout passe par le `count` des
    `Instantiator` désormais.
  - au boot, le host passe online **avant** confirmation de son pairState → l'auto-pair
    exige maintenant un état online+non-appairé+connu **stable 2,5 s** (rôle
    `statusUnknown` + timer d'armement), sinon il enverrait un PIN parasite à un host
    déjà appairé.
- ✅ **Profil de stream console** : 1080p / 60 fps / `max(défaut, 30 Mbps)` (cible §7),
  appliqué **au premier démarrage seulement** (marqueur `ConsoleUi/streamProfileInitialized`
  dans la conf) ; ensuite l'écran Paramètres fait foi.
- ✅ **Illusion console complétée** (`ConsoleDialog.qml` + `ConsoleSettings.qml`) :
  - **Y/X interceptés** dans `ConsoleHome` → ouvrent NOS Paramètres (résolution/fréquence/
    débit appliqués immédiatement + « Oublier ce PC ») ; sans ça les événements remontaient
    à `main.qml` qui ouvrait la SettingsView Material. B est consommé à l'accueil.
  - **Conflit « un autre jeu tourne »** : dialog console (Annuler par défaut) puis
    `QuitSegue` upstream réutilisé pour quitter-puis-enchaîner (`nextSession`).
  - **Retour de stream** : `StreamSegue.onDeactivating` ré-affiche la toolbar upstream →
    `ConsoleHome` re-masque le chrome à chaque `StackView.onActivated` (+ refocus carrousel).
  - **Batterie/signal factices retirés** de la StatusBar (masqués tant que pas de vraie
    source ; UPower/RSSI viendront avec le proto). Légende manette contextuelle.
- **Tâche en cours** : mode kiosk (boot direct, pas de bureau, §9 6c) + auto-accept du
  pairing côté host (rôle du futur installeur, §9 6b) pour que le code ne s'affiche jamais.

---

## 3. Architecture logicielle

L'insight central : **on n'écrit pas deux logiciels de zéro.** ~80 % existe déjà en open
source. Le vrai travail = **intégrer des briques existantes + ajouter une fine couche d'UX
et de packaging** pour supprimer la friction.

### Côté console (CE REPO)
- **Fork de moonlight-qt**, rebrandé, avec une **couche kiosk** : boot direct,
  auto-connexion, masquage de l'appairage, interface Big Picture maison.
- Principe : **« on n'écrit pas le streaming, on l'habille ».** Le décodage, le réseau,
  la manette sont déjà gérés par Moonlight. On retravaille surtout l'**UI** et le
  **comportement de démarrage**.
- OS cible : **Ubuntu Rockchip (Joshua Riek)** ou **Armbian** (kernel mainline,
  Mesa/Panfrost-Panthor pour le GPU, V4L2 pour le décodage), en mode kiosk.
- Compositeur : viser **gamescope** (celui de Valve/SteamOS : scaling, FSR, frame limiting,
  HDR) pour la version commerciale ; **cage** comme repli simple pour le proto.

### Côté PC host (logiciel séparé, à développer plus tard)
- Stack cible : **Apollo** (fork de Sunshine) + **Playnite** (agrège Steam/Epic/GOG/
  émulateurs avec jaquettes) + **PlayNiteWatcher** ou **Vibeshine** (export auto de la
  bibliothèque vers le host + box art) + **Tailscale** (accès distant, traversée du NAT).
- Le « logiciel host » à écrire = un **installeur/orchestrateur** qui pose et configure ces
  outils, tourne en arrière-plan, et gère l'**onboarding zéro-friction + l'auto-appairage**.
- (Setup actuel : le host « Djinger » tourne sous **Apollo** — l'appairage du fork a été
  validé contre Apollo en juin 2026. Sunshine reste compatible, même protocole.)

### Choix de lignée à garder en tête
- **Sunshine + moonlight-qt officiels** = plus stables, plus de plateformes.
- **Apollo + Artemis (Moonlight Noir)** = plus de fonctions de confort.
- Pour un handheld dédié, le compromis recommandé était **Apollo + un fork de moonlight-qt**.
  Ce repo part de moonlight-qt officiel ; rester compatible upstream est prioritaire (§4).

---

## 4. Stratégie de suivi d'upstream — LA convention de code n°1

On veut pouvoir **suivre les commits du repo officiel de moonlight** sans douleur. Donc :

- **Règle d'or : créer des fichiers neufs plutôt que réécrire les fichiers existants.**
  Un fichier neuf ne crée jamais de conflit de merge. Toute l'interface maison vit dans son
  propre dossier isolé : `app/gui/console/`.
- **Ne JAMAIS modifier un fichier upstream**, sauf les rares « coutures » strictement
  nécessaires, qu'on garde minuscules (voir §9).
- **Surtout, ne pas toucher `app/gui/main.qml`.**
- Git :
  - remote `upstream` = `https://github.com/moonlight-stream/moonlight-qt.git`
  - branche `master` = **miroir propre d'upstream** (on n'y code jamais)
  - branche `console-ui` = **tout le travail maison**
  - mise à jour : `git fetch upstream` → fast-forward `master` → `git rebase master` sur
    `console-ui` (historique linéaire, modifs rejouées proprement par-dessus l'officiel).
- Avant d'appliquer des changements : **toujours montrer les diffs.**

---

## 5. L'interface custom (direction artistique validée)

Style **Big Picture** (façon Steam), sombre et immersif, **100 % navigable à la manette**,
jamais d'élément "bureau". Choix esthétiques arrêtés :
- **Carrousel horizontal** de jeux, jaquette sélectionnée centrée, agrandie et **cerclée
  d'orange**, les voisines débordent et s'estompent.
- **Jaquettes au format 16:9** (= le format des captures que Sunshine/Moonlight fournit déjà).
- **Accent orange chaud** (`#F2802A`) pour le focus et le bouton Jouer ; fond très sombre.
- **État de connexion au PC host affiché en permanence** en haut (point vert + nom du host),
  pour que l'utilisateur voie d'un coup d'œil que tout marche — sans jamais voir d'IP ni de
  réglage réseau.
- **Légende manette** en bas, **contextuelle** (accueil : A Jouer · Y Paramètres ;
  dialog : A Valider · B Annuler ; paramètres : A Modifier · B Fermer).

Fichiers (dans `app/gui/console/`) :
- `ConsoleHome.qml` — l'écran d'accueil, assemble les 3 autres
- `GameCarousel.qml` — le carrousel 16:9 à focus orange
- `StatusBar.qml` — bandeau connexion (wordmark + chip à pastille pulsante) + signal/batterie/horloge
- `ControllerLegend.qml` — légende des boutons
- `PairingOverlay.qml` — écran de liaison (code PIN en grandes cases, façon app TV)
- `Spinner.qml` — indicateur d'activité circulaire réutilisable
- `ConsoleDialog.qml` — confirmation modale navigable manette (A/B, gauche/droite)
- `ConsoleSettings.qml` — écran Paramètres (bouton Y) : résolution/fréquence/débit/oubli du PC

`ConsoleHome.qml` est désormais **branché sur le vrai backend Moonlight** (imports
`ComputerModel`/`AppModel`/`ComputerManager`/`StreamingPreferences`) et n'est **plus
testable en autonome**. Les composants de présentation purs (`GameCarousel`, `StatusBar`,
`ControllerLegend`, `PairingOverlay`, `Spinner`) restent sans dépendance Moonlight et
peuvent se tester avec un `ListModel` de démo dans un harnais `qml-qt6` (penser à
`import "file:/chemin/vers/app/gui/console"` — les chemins absolus nus sont refusés).

---

## 6. Matériel

### Carte de développement : **Orange Pi 5B** (RK3588S), 8 Go / 64 Go eMMC (~106 €)
Choisie pour : décodage matériel H.264/HEVC/**AV1**, GPU Mali-G610 (Panthor/Panfrost),
**Wi-Fi 6 + BT 5.0 intégrés**, alim **USB-C 5V** (idéal handheld), eMMC soudée (boot fiable).
C'est une carte de **dev** : elle sert à valider RK3588S + le logiciel. Le produit final
sera un **SoM RK3588S** ou une **carte porteuse custom** autour de ce SoC.

### BOM du prototype (~270–300 €)
- Refroidissement actif (dissipateur + ventilo) — **obligatoire**, le RK3588S throttle sinon.
- Écran : démarrer en **HDMI IPS 5,5–6"** (marche tout seul) ; viser **MIPI-DSI** ensuite
  (le DSI sur Orange Pi est le **risque n°1** : panneau supporté dans le device-tree requis).
  Une dalle **AMOLED 7" 165 Hz MIPI** est envisagée (nécessite sa driver board).
- Contrôles : **RP2040 (Pi Pico) + firmware GP2040-CE** → manette USB-HID ;
  **joysticks à effet Hall** (anti-drift), D-pad, ABXY, gâchettes.
- Alim : **power bank USB-C PD 5V/3A+** pour le proto ; batterie custom (2× 18650 + boost
  type IP5389 5V/5A) seulement en v2.
- Wi-Fi : intégré pour commencer ; comparer avec un **dongle Wi-Fi 6 USB3 MediaTek**
  (mt7921/mt7925) si la latence déçoit.

### 5G (le « boss de fin », plus tard)
Module **Quectel RM520N-GL** (M.2, 5G Sub-6, Rel.16, bandes globales = OK France).
À écarter : RM530N-GL (certifié T-Mobile only), RM502Q-AE (fin de vie). Nécessite carte
porteuse + antennes + slot SIM. Le mur n'est pas le débit mais **latence + gigue + NAT**
(Tailscale gère le NAT).

---

## 7. Contraintes & décisions structurantes

- **Licence GPLv3** : Moonlight/Sunshine/Apollo sont en GPLv3. Toute modif distribuée doit
  être **publiée sous GPL avec les sources**. Le modèle « logiciel propriétaire fermé » est
  **impossible** ; le business viable = **hardware + service**.
- **Abstraire décodage / affichage / entrées** (V4L2 / DRM / SDL) et ne pas coupler au
  matériel en dur : on changera de SoC (dev → produit) presque sans douleur.
- **Certification radio CE/FCC** obligatoire dès qu'un produit vendu embarque Wi-Fi/5G.
- **Le Wi-Fi compte plus que le CPU** pour l'expérience de streaming. Viser ~30–50 Mbps
  *stables* à faible gigue en 1080p60.
- **Latence** ~20–60 ms : excellente pour les AAA solo, dépendante du réseau pour le FPS
  compétitif. Rester honnête là-dessus dans le positionnement.

---

## 8. Roadmap

1. ✅ Valider le concept de streaming (fait, depuis 1 an).
2. ✅ Forker + compiler moonlight-qt, décodage matériel OK.
3. ✅ Concevoir l'UI Big Picture (4 fichiers QML autonomes).
4. ✅ Intégrer l'UI dans le build derrière le flag `embedded` (commit `64bb96a0`).
5. ✅ Brancher l'UI sur les vraies données Moonlight (commits `e0f7600c` + `5df70d84`).
6. ⏳ **Mode kiosk + auto-appairage** (§9) — côté console : TERMINÉ (6a direct-launch,
   appairage auto avec code de liaison, Paramètres/dialogs console, chrome upstream
   masqué). Reste 6c (compositeur kiosk, lié au proto) et l'auto-accept côté host
   pour rendre le code invisible (lié à l'installeur, étape 8).
7. ⏳ Monter le proto hardware (Orange Pi 5B + écran + manette + power bank) —
   **carte commandée en juin 2026**. Premier objectif à réception : valider le décodage
   matériel 1080p60 (V4L2/FFmpeg sur RK3588S, kernel mainline + Panthor) — c'est LE
   risque technique restant du projet.
8. Installeur host 1-clic (Apollo/Sunshine + Playnite + Tailscale + Wake-on-LAN).
9. Version compacte v2 (SoM/carte custom, batterie, coque 3D).
10. Module 5G.

---

## 9. Tâche immédiate

La couche logicielle console est **fonctionnelle de bout en bout** (découverte →
appairage auto → carrousel → stream → retour carrousel, Paramètres au bouton Y).
Historique du découpage : 6a (direct-launch) ✅, 6b côté console (code de liaison,
aucun dialog upstream) ✅, 6c (kiosk) ⏳ hors repo.

Fronts ouverts, par priorité :

### A — Tests utilisateur de la couche console (en cours)
À valider à la manette par Marco : retour de stream sans toolbar, dialog de conflit
« un jeu tourne déjà », persistance des réglages Y, B inerte à l'accueil. Corriger ici
ce qui coince.

### B — 6c : boot direct via compositeur kiosk (à l'arrivée de l'Orange Pi)
Hors de ce repo : configurer **cage** (proto) ou **gamescope** (cible commerciale) pour
lancer `moonlight` au boot, sans session KDE/GNOME, sans curseur souris. Testable dès
maintenant sur le laptop Fedora (`cage -- ./app/moonlight`) ; à documenter en §11.
À réception de la carte : valider d'abord le **décodage matériel** (cf roadmap 7).

### C — Données réelles pour la StatusBar (avec le proto)
Brancher batterie (UPower) et signal Wi-Fi (RSSI 0-4) — les propriétés
`batteryPercent`/`signalStrength` existent déjà dans `StatusBar.qml` (-1 = masqué).
Nécessitera probablement un petit backend C++ dans `app/gui/console/` + une couture
`app.pro` dans le scope `embedded` existant.

### D — Auto-accept du pairing côté host (avec l'installeur, étape 8)
Pré-injecter l'identité/certificat de la console dans la conf d'Apollo (ou auto-accept),
pour que `PairingOverlay` ne s'affiche jamais en usage normal.

Contraintes (rappel §4) : tout le code console reste dans `app/gui/console/`. Aucun
fichier upstream touché. Build vanilla inchangé.

Vérification finale inchangée : sur un proto avec host paired, allumer la console doit
afficher le carrousel en moins de 5 s sans aucun élément Linux visible — et un jeu doit
pouvoir démarrer en une pression du bouton A.

---

## 10. Conventions de travail

- **Répondre en français.**
- **Toujours montrer les diffs** avant d'appliquer des changements.
- Garder les modifs **additives et isolées** ; préserver la compatibilité upstream (§4).
- En cas de doute sur un fichier upstream, demander plutôt que réécrire.

---

## 11. Notes de build durables

Pièges et conventions de build qui reviennent souvent — à garder sous la main.

### Comment compiler en mode `embedded` (UI maison)

```bash
qmake6 "CONFIG+=embedded" moonlight-qt.pro && make release -j$(nproc) && ./app/moonlight
```

L'app doit démarrer directement sur `ConsoleHome`. Un `qmake6` sans `embedded` doit
produire le Moonlight vanilla intact.

### Piège qmake `subdirs` (à connaître)

Si un précédent `qmake6` (sans `embedded`) a déjà généré les Makefiles des sous-projets,
relancer `qmake6 "CONFIG+=embedded" moonlight-qt.pro` au niveau racine **ne régénère pas**
les sous-Makefiles — le scope `embedded { ... }` reste inactif et `CONSOLE_UI` n'est pas
défini.

Symptômes : pas de `Project MESSAGE: Embedded build` au qmake, pas de `-DCONSOLE_UI` dans
la ligne `g++ -c ... main.cpp`, l'app démarre sur `PcView` malgré tout.

Deux contournements :
- `make distclean` avant le `qmake6` racine (propre mais recompile tout) ;
- ou régénérer directement le Makefile du sous-projet : `cd app && qmake6 "CONFIG+=embedded" app.pro`
  (rapide, recompile uniquement ce qui dépend du nouveau define).

### Piège ComputerModel/AppModel : initialiser AVANT d'assigner aux vues

`ComputerModel::initialize()` et `AppModel::initialize()` remplissent le modèle **sans
émettre de reset** (`beginResetModel`). Si une vue (ListView, Instantiator…) est liée au
modèle avant l'appel à `initialize()`, elle le considère **vide pour toujours** — symptôme :
« Recherche… » permanent alors que le host est en ligne. Le pattern correct (celui de
`PcView.qml` et de `ConsoleHome.qml`) :

```qml
var m = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', parent, '')
m.initialize(ComputerManager)   // d'abord initialiser…
computerModel = m               // …puis assigner la propriété liée aux vues
```

### Piège AppModel/ComputerModel : pas de propriété `count` en QML

Ces modèles C++ (QAbstractListModel) n'exposent **pas** de `count` : en QML,
`appModel.count` vaut `undefined`, donc `appModel.count > 0` est **toujours faux**
(symptôme : « Chargement de vos jeux… » infini alors que les données sont là).
Pour compter/observer les éléments, passer par un `Instantiator` lié au modèle et
utiliser **son** `count` (vraie propriété notifiée) — pattern des `hostScanner` /
`appViewer` de `ConsoleHome.qml`.

### Propriétés attachées : l'import compte

`StackView.onActivated` & co exigent `import QtQuick.Controls` dans le fichier qui les
utilise. Sans lui : « Non-existent attached object » au push, et la vue ne charge pas.

### Logs Qt invisibles sur Fedora (journald)

Quand stderr n'est pas un TTY, Qt envoie ses logs vers **journald** au lieu de la console
(`qml-qt6` n'affiche alors rien) → les lire avec `journalctl --user`. Le binaire
`moonlight` n'est pas concerné : il installe son propre handler (`qtLogToDiskHandler`)
qui écrit sur stdout. Attention : ce handler **supprime le niveau debug** (`console.log`
QML) — utiliser `console.info` pour qu'une trace QML apparaisse dans le log.

### rcc ne dépend que de `qml.qrc`, pas des `.qml`

Modifier un `.qml` ne suffit pas toujours à régénérer `qrc_qml.cpp` : faire
`touch qml.qrc` avant `make` pour forcer le réembarquement des ressources.

### Ignorer les artefacts de build sans toucher au `.gitignore` upstream

Le `.gitignore` upstream ne couvre pas les builds in-source de qmake (`Makefile*`,
`release/`, `*.o`, `*.a`, `.qmake.*`, `app/moonlight`, `config.log`…). Plutôt que de le
modifier (cf §4), on les ignore **localement à ce clone** via `.git/info/exclude` — fichier
non versionné, donc à **recréer après un `git clone`**. Contenu :

```
# qmake / make in-source build artifacts
Makefile
Makefile.Debug
Makefile.Release
release/
debug/
.qmake.cache
.qmake.stash

# Object files & static libs
*.o
*.a

# Build outputs / logs
config.log
app/moonlight
config.tests/*/EGL
```
