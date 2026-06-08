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
  Les 4 fichiers QML existent déjà dans `app/gui/console/` et tournent en aperçu autonome
  (`qml-qt6 ConsoleHome.qml`).
- ✅ **Intégration au build faite** (commit `64bb96a0` sur `console-ui`) : 3 coutures dans
  `app.pro`, `qml.qrc`, `main.cpp`, derrière `embedded`/`CONSOLE_UI`. Le binaire `embedded`
  démarre sur `ConsoleHome`, le binaire vanilla est inchangé.
- **Tâche en cours** : brancher l'UI sur les vraies données Moonlight
  (`appModel`/`computerModel`) et câbler le bouton Jouer sur la création de session (voir §9).

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
- (Setup actuellement utilisé au quotidien : **Sunshine** + moonlight-qt. Le passage à
  Apollo est une option, pas encore tranchée.)

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
- **Légende manette** en bas (A Jouer · B Retour · Y Paramètres).

Fichiers (dans `app/gui/console/`) :
- `ConsoleHome.qml` — l'écran d'accueil, assemble les 3 autres
- `GameCarousel.qml` — le carrousel 16:9 à focus orange
- `StatusBar.qml` — bandeau connexion + signal/batterie/horloge
- `ControllerLegend.qml` — légende des boutons

Ils sont **autonomes** (modèle de démo intégré, aucune dépendance au backend Moonlight),
donc testables avec `qml-qt6 ConsoleHome.qml`. Le **prochain gros chantier logiciel** sera
de remplacer le `ListModel` de démo par le **vrai catalogue** que Moonlight récupère du host
(`appModel`/`computerModel`) et de câbler le bouton Jouer sur la **vraie création de session**
(voir le commentaire `POINT D'INTÉGRATION MOONLIGHT` dans `ConsoleHome.qml`).

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
5. ⏳ **Brancher l'UI sur les vraies données Moonlight** (catalogue + lancement, §9).
6. Mode kiosk (boot direct, auto-connexion) + auto-appairage (supprimer le PIN de l'UX).
7. Monter le proto hardware (Orange Pi 5B + écran + manette + power bank).
8. Installeur host 1-clic (Apollo/Sunshine + Playnite + Tailscale + Wake-on-LAN).
9. Version compacte v2 (SoM/carte custom, batterie, coque 3D).
10. Module 5G.

---

## 9. Tâche immédiate

Brancher l'UI Big Picture sur les **vraies données Moonlight** au lieu du `ListModel` de
démo, puis câbler le bouton **Jouer** sur la vraie création de session.

État de départ :
- `ConsoleHome.qml` contient un `ListModel` de démo et un commentaire
  `POINT D'INTÉGRATION MOONLIGHT` qui marque l'emplacement à remplacer.
- Moonlight expose déjà côté backend `ComputerManager`, `ComputerModel` et `AppModel`
  (voir `app/backend/`). `PcView.qml` et `AppView.qml` du build vanilla montrent comment
  les consommer depuis QML.

Pistes (à confirmer par lecture de l'existant) :
1. Repérer comment `PcView.qml` instancie `ComputerModel` et reçoit la liste des hosts +
   le statut online/offline (pour alimenter `StatusBar.qml`).
2. Repérer comment `AppView.qml` instancie `AppModel` pour un host donné et reçoit la liste
   des jeux + box art (pour remplacer le `ListModel` de `GameCarousel.qml`).
3. Repérer comment le bouton « Démarrer » de `AppView.qml` enchaîne sur la création de
   session (`Session`, `StreamSegue`), et reproduire le même flux depuis `ConsoleHome.qml`.

Contraintes (rappel §4) :
- Ne modifier aucun fichier upstream. Si une donnée n'est pas exposée comme il faut, créer
  un *wrapper* QML/C++ dans `app/gui/console/` plutôt que de patcher l'existant.
- Le build vanilla doit rester intact.

Vérification finale : avec un host appairé en LAN, le carrousel doit afficher les vrais
jeux du host (avec leurs jaquettes), et le bouton Jouer doit lancer une session de stream.

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
