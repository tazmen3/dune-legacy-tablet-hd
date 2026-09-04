# Dune Legacy Tablet HD

**Dune Legacy Tablet HD** est un fork communautaire non officiel de
[Dune Legacy](https://dunelegacy.sourceforge.net/).

Le projet se concentre d'abord exclusivement sur Android et les tablettes. La
priorité absolue est d'obtenir une base Android ARM64 stable, reproductible et
facile à compiler, sans casser la compatibilité Windows et Linux héritée de
Dune Legacy.

## Première phase

La première phase vise uniquement à :

1. compiler correctement Dune Legacy pour Android ARM64 (`arm64-v8a`) ;
2. produire un APK installable ;
3. tester le jeu sur une tablette Android ;
4. vérifier qu'une partie complète fonctionne ;
5. identifier les problèmes liés à l'interface tactile ;
6. préparer ensuite une vraie interface pensée pour tablette.

Le dépôt amont ne fournit actuellement ni projet Gradle Android, ni cible APK,
ni procédure Android prête à l'emploi. Le premier jalon consiste donc à établir
et documenter une chaîne de compilation Android minimale autour du code
existant, avant toute modification fonctionnelle.

## Hors périmètre pour le moment

Aucune des fonctions avancées suivantes ne doit être développée pendant la
première phase :

- interface tactile complète ;
- sélection d'unités adaptée au doigt ;
- déplacement de caméra tactile ;
- pinch-to-zoom ;
- interface adaptée aux tablettes de 10 à 13 pouces ;
- rendu haute résolution ;
- nouveaux assets HD ;
- amélioration des détails graphiques ;
- modernisation du multijoueur et du lobby Internet.

Ces améliorations sont prévues pour des phases ultérieures, après validation
d'un APK ARM64 stable et d'une partie complète sur tablette.

## Principes de développement

- La branche `android` porte le travail Android initial.
- Les changements doivent rester minimaux et isolés par plateforme.
- Les builds Windows et Linux existants doivent continuer à fonctionner.
- Le code du jeu ne doit pas être remanié avant d'avoir caractérisé la
  compilation Android de la base actuelle.
- Les fichiers de données propriétaires de Dune II ne sont pas distribués par
  ce projet. Les contributeurs et utilisateurs doivent fournir leurs propres
  fichiers obtenus légalement.

## État initial de la cible Android

La base amont utilise CMake, C++17 et SDL2. Ses dépendances déclarées sont SDL2,
SDL2_mixer, SDL2_ttf, libcurl, miniupnpc et discord-rpc. Une chaîne Android
devra au minimum fournir le SDK Android, le NDK, CMake, Ninja, Gradle/JDK et des
versions Android compatibles de ces bibliothèques.

Avant d'ajouter du code tactile ou graphique, le premier travail technique sera
de vérifier chaque dépendance sur `arm64-v8a`, de désactiver proprement les
intégrations de bureau non pertinentes sur Android si nécessaire, puis
d'encapsuler l'exécutable SDL dans une application Android minimale.

## Projet amont et historique

Ce dépôt est basé sur le dépôt Git officiel de Dune Legacy :

- dépôt amont : <https://git.code.sf.net/p/dunelegacy/code> ;
- branche amont de référence : `master` ;
- version observée lors de la création du fork : `0.99.5` ;
- historique Git original conservé.

## Licence

Dune Legacy et ce fork sont distribués selon les termes de la GNU General
Public License, version 2 ou ultérieure (`GPL-2.0-or-later`). Consultez le
fichier [`COPYING`](COPYING) et les en-têtes des fichiers sources.

Les marques, noms, graphismes et données du jeu original restent la propriété
de leurs ayants droit. Ce projet n'est affilié ni à Westwood Studios, ni à
Electronic Arts, ni aux ayants droit de *Dune*.
