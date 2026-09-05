# Dune Legacy Tablet HD

**Dune Legacy Tablet HD** est un fork communautaire non officiel de
[Dune Legacy](https://dunelegacy.sourceforge.net/).

Le projet se concentre d'abord exclusivement sur Android et les tablettes. La
priorité absolue est d'obtenir une base Android ARM64 stable, reproductible et
facile à compiler, sans casser la compatibilité Windows et Linux héritée de
Dune Legacy.

## État actuel — commandes tactiles

La branche `touch-ui` contient l'adaptation tablette. L'APK
`0.99.5-android5` a été testé sur Samsung Galaxy Tab S9 le 5 septembre 2026 :
zoom, dézoom et déplacement de caméra fonctionnent selon le retour utilisateur,
sans ralentissement perceptible pendant ce test. L'affichage agrandi a également
été jugé confortable. Ce retour ne remplace pas un benchmark de performances ni
la validation d'une partie complète.

| Geste | Action |
| --- | --- |
| Tap à un doigt | Sélection ou action principale, boutons et menus |
| Glisser un doigt | Sélection rectangulaire, appliquée au relâchement |
| Glisser deux doigts ensemble | Déplacer la carte sous les doigts |
| Écarter deux doigts | Zoomer sur la carte |
| Rapprocher deux doigts | Dézoomer sur la carte |
| Maintenir un doigt environ 600 ms en jeu | Action du clic droit ; sur une icône de production, annuler directement un élément de la file |

Les gestes à deux doigts commencent sur la carte. Le zoom utilise les **trois
niveaux existants**, sans modifier la taille des menus et boutons. Le point sous
le centre du geste est conservé dans la limite des bords de carte.
Après retrait d'un doigt ou ajout d'un troisième, relâcher tous les doigts avant
de commencer un nouveau geste. La sélection rectangulaire n'a pas encore
d'aperçu continu. L'appui long est disponible à partir de `0.99.5-android6`
(validation sur tablette en attente) : maintenir le doigt sur l'icône de
construction dans la liste de production annule un élément sans passer par la
pause. Une seule annulation est envoyée par appui, sans clic gauche au
relâchement. Un mouvement ou un second doigt avant le délai annule l'appui long.
Sur la carte, l'appui long conserve l'action contextuelle du clic droit,
notamment l'annulation d'un mode de placement ; il ne démolit pas un bâtiment.

Pour compiler et installer l'APK, consulter [ANDROID_BUILD.md](ANDROID_BUILD.md).
Le [tableau Trello](https://trello.com/b/7mAO3AhC/dune-legacy-tablet-hd-modernisation-tablette)
est la référence du suivi du projet.

## Première phase — historique du cadrage initial

La première phase vise uniquement à :

1. compiler correctement Dune Legacy pour Android ARM64 (`arm64-v8a`) ;
2. produire un APK installable ;
3. tester le jeu sur une tablette Android ;
4. vérifier qu'une partie complète fonctionne ;
5. identifier les problèmes liés à l'interface tactile ;
6. préparer ensuite une vraie interface pensée pour tablette.

Le dépôt amont ne fournissait pas de chaîne APK prête à l'emploi lors de la
création du fork. Cette chaîne Android ARM64 est désormais disponible dans ce
dépôt ; l'adaptation tactile a ensuite commencé sur `touch-ui`.

## Étapes suivantes

Les améliorations restantes sont suivies dans Trello, notamment :

- validation de l'appui long et aperçu continu de sélection ;
- validation sur d'autres tablettes de 10 à 13 pouces ;
- rendu haute résolution ;
- nouveaux assets HD ;
- amélioration des détails graphiques ;
- modernisation du multijoueur et du lobby Internet.

La validation d'une partie complète reste à documenter.

## Principes de développement

- La branche `android` porte le travail Android initial.
- La branche `touch-ui` porte les commandes tactiles et le confort d'affichage.
- Les changements doivent rester minimaux et isolés par plateforme.
- Les builds Windows et Linux existants doivent continuer à fonctionner.
- Le code du jeu ne doit pas être remanié avant d'avoir caractérisé la
  compilation Android de la base actuelle.
- Les fichiers de données propriétaires de Dune II ne sont pas distribués par
  ce projet. Les contributeurs et utilisateurs doivent fournir leurs propres
  fichiers obtenus légalement.

## Base technique Android

La base amont utilise CMake, C++17 et SDL2. Ses dépendances déclarées sont SDL2,
SDL2_mixer, SDL2_ttf, libcurl, miniupnpc et discord-rpc. La chaîne Android utilise
le SDK Android, le NDK, CMake, Ninja et Gradle/JDK ; les versions exactes sont
documentées dans [ANDROID_BUILD.md](ANDROID_BUILD.md).

Le moteur est empaqueté dans une application SDL Android pour `arm64-v8a`.
Discord Rich Presence est désactivé sur Android ; miniupnpc reste disponible.

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
