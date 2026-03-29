# Analyse des dépendances — BBFx

> Inventaire complet de toutes les dépendances de BBFx, leur version d'origine, leur statut en 2026 et la stratégie recommandée pour chacune dans le cadre de la remise en fonctionnement.

---

## Résumé exécutif

| Dépendance | Version 2006 | Statut 2026 | Criticité | Action requise |
|---|---|---|---|---|
| **OGRE 3D** | 1.2.x (Dagon) | Ancien, sources disponibles | **Critique** | Builder 1.4.9 depuis sources |
| **OgreLua** | Interne (SWIG) | Abandonné | **Critique** | Régénérer avec SWIG 4.x |
| **Lua** | 5.1 | Stable, encore disponible | Faible | Utiliser la version embarquée ou apt |
| **SWIG** | 1.3.29 | Obsolète, 4.4.1 disponible | Moyen | Migrer vers SWIG 4.x |
| **OIS** | ~0.9 | **Activement maintenu** (v1.6.0) | Faible | Utiliser OIS 1.6.0 |
| **Boost.Graph** | ~1.33 | Stable, inclus dans Boost | Faible | Boost 1.80+ compatible |
| **libjsw** | ~2.x | Abandonné, Linux-only | Élevé | Remplacer par SDL2 |
| **pthread** | POSIX | Linux natif / winpthreads | Faible | Natif WSL2 / winpthreads MSYS2 |
| **SCons** | ~0.96 | 4.8.x (Python 3) | Faible | Migrer ou fixer print statements |
| **g++** | ~4.0 | GCC 11-13 | Moyen | Utiliser `-std=c++14` |
| **X11/OpenGL** | Linux natif | WSLg (Win10) / natif Linux | Moyen | WSL2 + WSLg recommandé |

---

## 1. OGRE 3D

### Contexte

OGRE (Object-Oriented Graphics Rendering Engine) est le moteur 3D central de BBFx. Toute la gestion de la scène, du rendu, des animations de maillage, des matériaux et des effets repose sur OGRE. Il est impossible de faire fonctionner BBFx sans une version compatible d'OGRE.

### Version utilisée en 2006

**OGRE 1.2.x « Dagon »** — Le code source de BBFx (SConstruct, config.lua) et le nom de répertoire `2006.06.10` indiquent clairement que le projet a été développé sur la base d'OGRE 1.2.x. OGRE 1.2.0 a été publié le 7 mai 2006, soit environ un mois avant le snapshot du projet. Le fichier `TODO` du projet contient une section intitulée `DAGON` qui référence des API OGRE spécifiques à cette version.

### Statut en 2026

OGRE est toujours activement développé. La version actuelle est **OGRE-Next 14.x** (anciennement OGRE 2.x), avec des changements d'API majeurs et incompatibles avec OGRE 1.x.

La branche **OGRE 1.x (OGRECave/ogre, branche v1-13)** est maintenue séparément sur GitHub mais reçoit peu de mises à jour. Elle préserve l'API 1.x mais introduit des changements C++ modernes incompatibles avec le code de 2006.

**Versions recommandées pour la compatibilité :**

| Version | Date | Notes |
|---|---|---|
| **1.4.9** | Juin 2008 | Dernière de la série 1.4 « Eihort ». API superset de 1.2. **Recommandée.** |
| 1.2.5 | Janvier 2007 | Plus proche de l'original, mais moins de bugfixes |
| 1.9.0 | 2013 | Beaucoup de bugfixes modernes mais plus de changements API |

### Disponibilité des sources

- **OGRE 1.4.9 source** : `https://sourceforge.net/projects/ogre/files/ogre/1.4.9/`
  - `ogre-v1-4-9.tar.bz2` (29,6 Mo) — Linux/source
  - `OgreSDKSetup1.4.9_VC90.exe` — SDK précompilé Visual C++ 2008

- **OGRE 1.2.5 source** : `https://sourceforge.net/projects/ogre/files/ogre/1.2.x/`

### Compatibilité Windows 10

OGRE 1.4.x peut être compilé sur Windows 10 avec :
- **MSYS2/MinGW-w64 + GCC** : nécessite de compiler depuis les sources avec `-std=c++14` pour gérer les dépréciations C++
- **Visual Studio 2019** (avec SDK VC90 précompilé) : plus complexe à mettre en place, runtime C++ différent
- **WSL2** : compilation native Linux sans changements — **chemin le plus simple**

**Problèmes connus :**
- `std::auto_ptr` : utilisé massivement dans OGRE 1.x, supprimé en C++17. Compiler avec `-std=c++14` obligatoire.
- Intel GPU sous Windows : support OpenGL médiocre via les drivers Intel. Préférer le renderer Direct3D si GPU NVIDIA/AMD.
- GCC 13 génère des warnings/erreurs sur code C++03. Flag `-std=c++14` ou `-std=gnu++14` nécessaire.

### Action recommandée

1. Télécharger OGRE 1.4.9 source depuis SourceForge
2. Compiler sous WSL2 Ubuntu 22.04 avec `cmake -DCMAKE_CXX_STANDARD=14`
3. Installer dans `/usr/local/` pour que les SConstruct existants trouvent les headers

---

## 2. OgreLua (Bindings SWIG OGRE → Lua)

### Contexte

OgreLua est le composant **le plus problématique** de la migration. Il s'agit du projet qui expose l'API OGRE complète à Lua via des bindings SWIG. Le projet contient plus de 200 fichiers `.i` SWIG couvrant l'intégralité de l'API OGRE.

### Version utilisée en 2006

Projet interne développé spécifiquement pour BBFx, présent dans `workspace/ogrelua/`. Pas de version publique — les bindings ont été écrits manuellement pour OGRE 1.2.x avec SWIG 1.3.29.

### Statut en 2026

- Le projet OgreLua interne est **abandonné** et non maintenu.
- Un dépôt GitHub `alextrevisan/ogrelua` existe mais est également abandonné (2014, 27 commits, aucune release).
- OGRE officiel (OGRECave) expose des bindings SWIG pour Python, C# et Java, mais **pas pour Lua**.
- Il n'existe **aucun binding Lua maintenu pour OGRE 1.x en 2026**.

### Chemins de migration

#### Option A — Régénérer avec SWIG 4.x (Recommandée)

Les fichiers `.i` existants dans `ogrelua/swig/` sont compatibles avec SWIG 4.x avec des modifications mineures :

**Changements breaking connus SWIG 1.3 → 4.x pour Lua :**
1. Espaces de noms C++ : maintenant mappés à des tables Lua (déjà gérés dans les .i existants)
2. Variables immutables : SWIG 4.x génère une erreur si on tente de modifier une constante. Compiler avec `-DSWIGLUA_IGNORE_SET_IMMUTABLE` pour restaurer l'ancien comportement.
3. `%module` avec options : syntaxe étendue dans SWIG 4.x, rétrocompatible.
4. `lua_isfunction` dans typemaps : API Lua inchangée entre 5.1 et 5.4.

**Procédure :**
```bash
swig -4.4.1 -c++ -lua -v -I/path/to/ogre/include \
     -DSWIGLUA_IGNORE_SET_IMMUTABLE \
     ogrelua/swig/Ogre.i
# Corriger les warnings générés
# Compiler le wrapper résultant
```

#### Option B — Réécrire avec sol2 (Effort important)

[sol2](https://github.com/ThePhD/sol2) est la bibliothèque C++/Lua la plus moderne et la plus utilisée. Elle permettrait de réécrire les bindings en C++17 moderne sans SWIG. Avantages : meilleure performance, API plus intuitive. Inconvénient : réécriture complète des 200+ bindings.

#### Option C — Utiliser LuaBridge3 (Effort modéré)

[LuaBridge3](https://github.com/kunitoki/LuaBridge3) est une alternative légère à sol2 compatible avec Lua 5.1-5.4. Moins de travail que sol2 pour des bindings simples.

### Action recommandée

**Option A** pour une remise en fonctionnement rapide. Si le projet doit être maintenu à long terme, envisager une migration vers **sol2 (Option B)** dans une seconde phase.

---

## 3. Lua

### Version utilisée en 2006

**Lua 5.1** — Runtime embarqué dans `prog/lua/5.1/` (headers + libray compilée Linux).

### Statut en 2026

Lua 5.1 est **toujours disponible** et largement distribuée. La version courante de Lua est 5.4.7 (2024). Lua 5.1 est maintenu pour compatibilité dans de nombreux systèmes embarqués et jeux vidéo.

**Compatibilité ascendante :** Lua 5.2 a introduit des changements incompatibles (suppression de `setfenv`/`getfenv`, changement de `module()`) mais le code de BBFx utilise Lua 5.1 sans fonctionnalités dépréciées problématiques. Le code est compatible avec Lua 5.1 natif sans modification.

### Disponibilité

```bash
# Ubuntu/WSL2
sudo apt install liblua5.1-dev lua5.1

# MSYS2
pacman -S mingw-w64-x86_64-lua51
```

Source : `https://www.lua.org/ftp/lua-5.1.5.tar.gz`

### Action recommandée

Utiliser **Lua 5.1.5** (dernière version de la série 5.1). Les binaires Linux disponibles dans le snapshot peuvent être réutilisés dans WSL2 si l'ABI est compatible. Sinon, utiliser le package `liblua5.1-dev` de la distribution.

---

## 4. SWIG

### Version utilisée en 2006

**SWIG 1.3.29** — Compilateur embarqué dans `prog/swig-1.3.29/` (sources C + binaire Linux `swig`).

### Statut en 2026

SWIG est **activement maintenu**. Version courante : **SWIG 4.4.1** (2025), disponible sur `https://www.swig.org/download.html`.

**Chronologie des versions importantes :**
- 1.3.40 (2010) : dernière de la série 1.3
- 2.0.0 (2010) : première version avec changement de licence (GPL → un choix entre GPL et LGPL)
- 3.0.0 (2014) : support C++11
- 4.0.0 (2019) : support C++17, refonte de la documentation
- 4.4.1 (2025) : version courante

**Compatibilité des fichiers .i existants :** Les fichiers `bbfx.i` et les fichiers `ogrelua/swig/*.i` sont écrits pour SWIG 1.3.29. Ils génèrent des **warnings** avec SWIG 4.x mais compilent généralement sans erreurs bloquantes. Les principaux ajustements nécessaires :
- Remplacer `%import` par `%include` si nécessaire
- Vérifier les directives `%exception` (syntaxe inchangée)
- Ajouter `-DSWIGLUA_IGNORE_SET_IMMUTABLE` si besoin

### Disponibilité Windows

```bash
# WSL2/Ubuntu
sudo apt install swig

# MSYS2
pacman -S mingw-w64-x86_64-swig

# Windows natif (exécutable précompilé)
# https://sourceforge.net/projects/swig/files/swig/swig-4.4.1/swigwin-4.4.1.zip
```

### Action recommandée

Utiliser **SWIG 4.4.1**. Le binaire SWIG 1.3.29 embarqué dans le snapshot est un binaire Linux ELF 32-bit — il peut fonctionner sous WSL2 si les bibliothèques 32-bit sont installées, mais il est préférable d'utiliser SWIG 4.x.

---

## 5. OIS (Open Input System)

### Version utilisée en 2006

**OIS ~0.9** — Sources présentes dans `workspace/ois/` (selon la configuration du SConstruct). Origine : SourceForge, projet `wgois`.

### Statut en 2026

OIS est **activement maintenu**. Migré de SourceForge vers **GitHub** (`https://github.com/wgois/OIS`).

**Dernière version : v1.6.0 (27 février 2025)**

Changements majeurs depuis 2006 :
- Build system migré de autotools → **CMake**
- Support XInput (Windows, vibrations)
- Améliorations des joysticks Linux
- API de base (création de InputManager, Keyboard, Mouse, Joystick) **inchangée et compatible avec BBFx**

### Installation

```bash
# Ubuntu/WSL2 (peut ne pas être dans les dépôts officiels)
git clone https://github.com/wgois/OIS
cd OIS && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make && sudo make install

# MSYS2
pacman -S mingw-w64-x86_64-OIS  # si disponible
# Sinon : compiler depuis les sources avec cmake
```

### Considérations Windows

Sur Windows natif, OIS utilise **DirectInput** pour clavier/souris et **XInput** pour manettes. Le passage de `HWND` (handle de fenêtre Windows) à la place du `Window handle` X11 est nécessaire — OGRE 1.4.x gère cela automatiquement via `getRenderWindow()->getCustomAttribute("HWND", &hwnd)`.

### Action recommandée

Utiliser **OIS v1.6.0** depuis GitHub. L'API est rétrocompatible avec la version 2006. Compiler depuis les sources avec CMake sous WSL2 ou MSYS2.

---

## 6. libjsw (Linux Joystick Wrapper)

### Contexte

libjsw est utilisé dans `Joystick.h/cpp` pour la lecture des joysticks Linux via `/dev/js*`. C'est la **seule dépendance strictement Linux-only** de BBFx.

### Version utilisée en 2006

libjsw ~2.x. La bibliothèque est référencée via `-ljsw` dans le SConstruct. Les fichiers de calibration `joystick.044f_b303` et `joystick.045e_0028` présents dans le snapshot attestent d'une utilisation avec deux manettes physiques (044f = Thrustmaster, 045e = Microsoft/Xbox).

### Statut en 2026

libjsw est **abandonné**. Pas de développement actif depuis les années 2000. Dépôt GitHub miroir : `https://github.com/herrsergio/libjsw`. Dernière modification : 2003.

**Portabilité :** Strictement Linux. Utilise directement `/dev/js*` via `<linux/joystick.h>` qui n'existe pas sur Windows (même via WSL2 — la prise en charge des joysticks USB en WSL2 nécessite `usbipd-win`).

### Alternatives

| Alternative | Plateforme | Notes |
|---|---|---|
| **SDL2** (`SDL_Joystick`) | Linux, Windows, macOS | Cross-platform, mature, API simple |
| **libenjoy** | Linux, Windows | Petit wrapper minimaliste, proche de libjsw conceptuellement |
| **Windows Multimedia API** (`joyGetPosEx`) | Windows uniquement | Disponible dans `winmm`, zéro dépendance |
| **XInput** | Windows uniquement | Microsoft, manettes Xbox et compatibles |
| **Linux kernel joystick** directement | Linux uniquement | Sans libjsw, utiliser `<linux/joystick.h>` directement |

**API libjsw utilisée dans BBFx (surface minimale à remplacer) :**
```c
JSGetAttributesList()           // détection des joysticks disponibles
JSOpen(device, calibFile)       // ouverture avec calibration
JSClose()                       // fermeture
JSUpdate(js)                    // lecture non-bloquante de l'état
js->axis[i]                     // valeur d'axe normalisée (-1.0 à 1.0)
js->button[i]                   // état bouton (0/1)
```

Cette surface est très réduite → remplacement par SDL2 estimé à **200-300 lignes** de C++.

### Action recommandée

**WSL2** : libjsw peut fonctionner si le joystick USB est passé à la VM via `usbipd-win`. Alternative : remplacer par SDL2 pour une solution plus robuste.

**Windows natif** : remplacer par **SDL2** (cross-platform) ou Windows Multimedia API (plus simple si support Xbox non requis).

```bash
# Dépendance SDL2
# Ubuntu/WSL2
sudo apt install libsdl2-dev

# MSYS2
pacman -S mingw-w64-x86_64-SDL2
```

---

## 7. Boost Graph Library

### Version utilisée en 2006

**Boost ~1.33** — Utilisé via `<boost/graph/adjacency_list.hpp>` et `<boost/graph/graph_traits.hpp>`.

### Statut en 2026

Boost est **activement maintenu**. Version courante : Boost 1.87 (2025). La Boost Graph Library (BGL) est une composante stable de Boost dont l'API n'a pas changé de façon incompatible depuis 2006.

**Compatibilité :** Le code `Animator.h` utilise des features BGL basiques (`adjacency_list`, `graph_traits`, `property_map`) qui sont inchangées entre Boost 1.33 et 1.87.

### Disponibilité

```bash
# Ubuntu/WSL2
sudo apt install libboost-graph-dev

# MSYS2
pacman -S mingw-w64-x86_64-boost
```

### Action recommandée

Utiliser la version de Boost disponible dans le gestionnaire de paquets. Aucune modification du code BBFx n'est requise.

---

## 8. POSIX Threads (pthread)

### Version utilisée en 2006

`pthread` natif Linux — utilisé dans `core/Mutex.h/cpp` pour les mutex récursifs (`PTHREAD_MUTEX_RECURSIVE_NP`).

### Statut en 2026

POSIX pthreads est inchangé. La constante `PTHREAD_MUTEX_RECURSIVE_NP` est la version **non-portable** (NP = Non Portable) de `PTHREAD_MUTEX_RECURSIVE`. Elle fonctionne sur Linux mais pas nécessairement sur d'autres systèmes.

### Portabilité Windows

| Environnement | Solution pthread |
|---|---|
| **WSL2** | Native glibc pthreads — aucun changement requis |
| **MSYS2/MinGW** | `winpthreads` (inclus par défaut, flag `-pthread`) |
| **MSVC** | `pthreads4w` (`https://sourceforge.net/projects/pthreads4w/`) ou migration vers `std::mutex` |

**Modification mineure requise :**
```cpp
// Avant (Linux-only)
PTHREAD_MUTEX_RECURSIVE_NP

// Après (portable POSIX)
PTHREAD_MUTEX_RECURSIVE
```

### Action recommandée

Sous WSL2 : aucun changement requis. Remplacer `PTHREAD_MUTEX_RECURSIVE_NP` par `PTHREAD_MUTEX_RECURSIVE` pour la portabilité.

---

## 9. SCons

### Version utilisée en 2006

**SCons ~0.96** — Python 2, scripts en `prog/workspace/bbfx/src/SConstruct` et `ogrelua/SConstruct`.

### Statut en 2026

SCons est **activement maintenu**. Version courante : **SCons 4.8.x** (Python 3.7+).

**Problèmes de compatibilité Python 2 → Python 3 :**

Les SConstruct de BBFx utilisent des constructions Python 2 qui doivent être corrigées :

| Construction Python 2 | Correction Python 3 |
|---|---|
| `print "string"` | `print("string")` |
| `dict.has_key(k)` | `k in dict` |
| `dict.iteritems()` | `dict.items()` |
| `string` type → `unicode` | aucun changement dans les SConstruct BBFx |

**Les SConstruct de BBFx sont simples** et ne devraient nécessiter que 2-3 corrections.

**Alternative : Python 2 + SCons 2.x**
```bash
# Ubuntu : python2.7 encore disponible en 22.04
sudo apt install python2
pip2 install scons==2.5.1
python2 -m SCons
```

### Action recommandée

Corriger les 2-3 `print` statements dans les SConstruct pour les rendre compatibles Python 3, puis utiliser SCons 4.x.

---

## 10. g++ / GCC

### Version utilisée en 2006

**GCC ~4.0** sur Linux x86 — Flags: `-O2 -march=athlon` (processeur AMD Athlon).

### Statut en 2026

GCC courant : **GCC 13.x** (Ubuntu 22.04 fournit GCC 11.x). Nombreuses évolutions du compilateur qui affectent le code de 2006 :

| Problème | Impact | Solution |
|---|---|---|
| `std::auto_ptr` supprimé en C++17 | Bloquant | Compiler avec `-std=c++14` |
| Conversions implicites C++ plus strictes | Warnings/Erreurs | Corriger au cas par cas |
| `-march=athlon` obsolète sur x86_64 | Warning | Remplacer par `-march=native` ou supprimer |
| Headers dépréciés (`<ext/hash_map>` etc.) | Potentiel | Vérifier dans OGRE 1.4.9 |
| `register` keyword interdit en C++17 | Erreur | Compilation en C++14 |

**Flag de compilation recommandé pour OGRE 1.4.9 + BBFx :**
```bash
CXXFLAGS="-std=c++14 -O2 -march=native -w"
# -w : supprime les warnings (code ancien génère beaucoup de bruit)
# Enlever -w pour voir les problèmes réels
```

---

## 11. X11 / OpenGL / Système de fenêtrage

### Contexte

BBFx dans sa configuration d'origine utilise :
- X11 pour la gestion de fenêtre (paramètre `WINDOW` passé à OIS via `getCustomAttribute("WINDOW")`)
- OpenGL comme renderer OGRE (configuré dans `config.lua` : `"RenderSystem" = "OpenGL Rendering Subsystem"`)
- Plugin OGRE `RenderSystem_GL`

### Sur Windows 10 en 2026

Trois approches :

#### WSL2 + WSLg (Recommandée)
WSLg (Windows Subsystem for Linux GUI) est intégré dans Windows 10 (21H2+). Il fournit :
- Serveur XWayland automatiquement démarré
- Accélération OpenGL via la couche de traduction **Mesa D3D12** (OpenGL → DirectX 12)
- Pas de configuration X11 supplémentaire nécessaire

**Limites :** Performance OpenGL légèrement réduite par la couche de traduction. Pour des effets complexes, peut ne pas être suffisant.

#### X410 ou VcXsrv (Serveur X11 Windows)
Serveurs X11 natifs Windows pour WSL2 sans WSLg :
- **VcXsrv** : `https://vcxsrv.com/` — gratuit, mature
- **X410** : payant (Windows Store), meilleures performances
- Configurer `DISPLAY=:0` dans WSL2

#### OGRE avec Windows DirectX
En remplaçant X11 par Win32, OGRE peut utiliser DirectX nativement. Nécessite de modifier le `config.lua` pour utiliser `"Direct3D9 Rendering Subsystem"` ou `"Direct3D11 Rendering Subsystem"` et de compiler sous MSYS2 ou MSVC.

---

## 12. Dépendances médias et ressources

### Assets OGRE (`prog/bbfx.media/`)

| Répertoire | Contenu |
|---|---|
| `Bootstrap/` | Ressources OGRE core (fonts, overlays système) — fichiers ZIP |
| `General/` | Assets de la démo : `ogrehead.mesh`, matériaux, textures, particules |
| `gui/` | Overlays GUI personnalisés |

**Statut :** Ces assets sont présents dans le snapshot et sont dans des formats OGRE 1.x standard (`.mesh`, `.material`, `.compositor`, `.particle`). Ils sont **directement réutilisables** avec OGRE 1.4.9 sans modification.

### Ressources manquantes identifiées

Certaines références dans les scripts Lua pointent vers des ressources qui pourraient ne pas être dans le snapshot :
- `geosphere8000.mesh` (référencé dans `test-perlin.lua`)
- `robot.mesh` (exemple dans `scenespec.lua`)
- Certains matériaux GLSL (shaders Perlin)

Ces ressources sont disponibles dans les packs de démo d'OGRE 1.x.

---

## 13. Theora (Plugin vidéo)

### Contexte

Le plugin Theora est référencé dans `bbfx.lua` (`require "plugins.theora"`) et `test-theora/` dans le workspace. Il n'est pas compilé dans la version archivée du projet.

### Dépendances Theora

- `libtheora` : décodeur vidéo libre (codecs Ogg/Theora)
- `libogg` : container Ogg
- Plugin OGRE Theora : `workspace/test-theora/`

### Statut 2026

libtheora est disponible (`sudo apt install libtheora-dev`). Le plugin OGRE Theora devra probablement être recompilé depuis les sources. Recommandé de le **mettre en commentaire** lors de la phase initiale de remise en fonctionnement et de l'activer dans une seconde phase.

---

## Récapitulatif des actions par priorité

### Priorité haute (bloquant)
1. **OGRE 1.4.9** : télécharger les sources, compiler sous WSL2
2. **OgreLua** : régénérer les bindings avec SWIG 4.x

### Priorité moyenne (nécessaire)
3. **libjsw** : remplacer par SDL2 (200-300 lignes de C++)
4. **SConstruct** : adapter les chemins et corriger la syntaxe Python 3
5. **SWIG** : installer 4.4.1 et régénérer

### Priorité faible (mineure)
6. **OIS** : compiler OIS 1.6.0 depuis GitHub
7. **pthread** : remplacer `PTHREAD_MUTEX_RECURSIVE_NP` par `PTHREAD_MUTEX_RECURSIVE`
8. **Flags GCC** : remplacer `-march=athlon` par `-march=native`

### Non bloquant pour la phase initiale
9. **Theora** : désactiver dans `bbfx.lua` pour les tests initiaux
10. **Plugin FX** : `fx/` n'est pas compilé dans la version archivée (commenté dans SConstruct)

---

*Document établi en mars 2026. Sources : analyse statique du code source BBFx, recherches web sur l'état des projets en 2026.*
