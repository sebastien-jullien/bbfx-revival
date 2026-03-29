# Guide de mise en place de l'environnement — BBFx 2026

> Procédure pas-à-pas pour reconstruire l'environnement d'exécution de BBFx sur Windows 10 en 2026. Deux stratégies sont documentées : WSL2 (recommandée, minimise les modifications du code) et MSYS2/MinGW (binaire Windows natif, effort plus important).

---

## Stratégie recommandée : WSL2 + WSLg

WSL2 (Windows Subsystem for Linux 2) permet d'exécuter un noyau Linux complet sous Windows 10. WSLg (Windows Subsystem for Linux GUI) ajoute le support des applications graphiques X11 avec accélération matérielle via DirectX 12. C'est l'approche qui nécessite le **moins de modifications du code source** de BBFx.

**Prérequis Windows :**
- Windows 10 version 21H2 minimum (Build 19044+) pour WSLg
- GPU avec drivers récents (NVIDIA, AMD ou Intel)
- Au moins 8 Go de RAM (16 Go recommandés)
- 20 Go d'espace disque disponible

---

## Partie 1 — Installation de WSL2

### 1.1 Activer WSL2 et installer Ubuntu 22.04

Ouvrir PowerShell en administrateur :

```powershell
# Installation complète WSL2 + Ubuntu 22.04 (WSLg inclus)
wsl --install -d Ubuntu-22.04

# Vérifier la version WSL après installation
wsl --version

# Si WSL est déjà installé, mettre à jour
wsl --update
```

Après redémarrage, créer un utilisateur Unix et un mot de passe lors du premier lancement d'Ubuntu.

### 1.2 Vérifier WSLg (support graphique)

```bash
# Dans le terminal Ubuntu WSL2
echo $DISPLAY
# Doit afficher quelque chose comme :0 ou :1

# Test rapide avec une application X11
sudo apt install x11-apps -y
xclock &
# Une horloge graphique doit s'afficher dans une fenêtre Windows
```

Si WSLg ne fonctionne pas, installer **VcXsrv** manuellement :
- Télécharger : `https://vcxsrv.com/`
- Lancer XLaunch avec "Multiple windows", "Start no client", cocher "Disable access control"
- Dans WSL2 : `export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0`

---

## Partie 2 — Installation des dépendances système

### 2.1 Mise à jour et outils de base

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y \
    build-essential \
    cmake \
    python3 \
    python3-pip \
    git \
    wget \
    curl \
    pkg-config \
    autoconf \
    automake \
    libtool \
    unzip
```

### 2.2 Bibliothèques de développement

```bash
sudo apt install -y \
    libboost-all-dev \
    libboost-graph-dev \
    libx11-dev \
    libxrandr-dev \
    libxt-dev \
    libxaw7-dev \
    libxmu-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libgl-dev \
    libglew-dev \
    libfreetype-dev \
    libzzip-dev \
    libpng-dev \
    libjpeg-dev \
    libfreeimage-dev \
    libsdl2-dev \
    liblua5.1-dev \
    lua5.1
```

### 2.3 Installer SCons

```bash
pip3 install scons
# Vérifier
scons --version
```

### 2.4 Installer SWIG 4.x

```bash
sudo apt install swig
# Vérifier la version
swig -version
# Si la version apt est trop ancienne, compiler depuis les sources :
# wget https://sourceforge.net/projects/swig/files/swig/swig-4.4.1/swig-4.4.1.tar.gz
# tar xzf swig-4.4.1.tar.gz && cd swig-4.4.1
# ./configure && make && sudo make install
```

---

## Partie 3 — Accès au snapshot BBFx depuis WSL2

Le snapshot BBFx est sur le disque Windows (`E:\prog\2006.06.10 (Gron)`). Dans WSL2, les disques Windows sont montés sous `/mnt/` :

```bash
# Accéder au snapshot depuis WSL2
ls "/mnt/e/prog/2006.06.10 (Gron)/prog/workspace/"

# Créer un lien symbolique pour plus de commodité
ln -s "/mnt/e/prog/2006.06.10 (Gron)" ~/bbfx-snapshot

# Travailler depuis un répertoire de travail local (recommandé pour les performances)
cp -r ~/bbfx-snapshot ~/bbfx-work
cd ~/bbfx-work
```

> **Note performance :** Les accès aux fichiers Windows depuis WSL2 via `/mnt/` sont significativement plus lents que sur le filesystem Linux natif. Il est fortement recommandé de copier les sources dans `~/` pour le build.

---

## Partie 4 — Compilation d'OGRE 1.4.9

C'est l'étape la plus longue mais la plus critique.

### 4.1 Télécharger les sources OGRE 1.4.9

```bash
mkdir -p ~/deps && cd ~/deps
wget "https://sourceforge.net/projects/ogre/files/ogre/1.4.9/ogre-v1-4-9.tar.bz2/download" \
     -O ogre-v1-4-9.tar.bz2
tar xjf ogre-v1-4-9.tar.bz2
cd ogre-v1-4-9
```

### 4.2 Vérifier les dépendances OGRE

OGRE 1.4.9 nécessite ces bibliothèques (normalement déjà installées à l'étape 2.2) :
```bash
# Vérifier
pkg-config --exists gl && echo "GL OK"
pkg-config --exists freetype2 && echo "FreeType OK"
dpkg -l | grep libzzip  && echo "zzip OK"
dpkg -l | grep libpng   && echo "PNG OK"
dpkg -l | grep libjpeg  && echo "JPEG OK"
```

### 4.3 Configuration du build OGRE

OGRE 1.4.9 utilise CMake (ou les anciens scripts autoconf) :

```bash
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_STANDARD=14 \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DOGRE_BUILD_SAMPLES=FALSE \
    -DOGRE_BUILD_TOOLS=FALSE \
    -DOGRE_BUILD_TESTS=FALSE \
    -DOGRE_INSTALL_DOCS=FALSE \
    -DOGRE_INSTALL_SAMPLES=FALSE

# Compiler (utiliser tous les cœurs disponibles)
make -j$(nproc)

# Installer
sudo make install
sudo ldconfig
```

> **Si cmake n'est pas disponible pour OGRE 1.4.9**, utiliser les scripts autoconf :
> ```bash
> ./bootstrap
> ./configure --prefix=/usr/local CXX="g++" CXXFLAGS="-std=c++14 -O2"
> make -j$(nproc)
> sudo make install
> ```

### 4.4 Vérifier l'installation OGRE

```bash
ls /usr/local/include/OGRE/
# Doit lister : OgreRoot.h, OgreSceneManager.h, etc.

ls /usr/local/lib/ | grep Ogre
# Doit lister : libOgreMain.so, libOgreMain.so.1.4.9, etc.

pkg-config --cflags OGRE 2>/dev/null || echo "pkg-config OGRE non configuré"
```

### 4.5 Configurer les plugins OGRE

Créer ou vérifier le fichier `plugins.cfg` dans le répertoire de travail :
```ini
# plugins.cfg (chemin adapté à l'installation /usr/local)
PluginFolder=/usr/local/lib/OGRE

Plugin=RenderSystem_GL
Plugin=Plugin_OctreeSceneManager
Plugin=Plugin_ParticleFX
```

---

## Partie 5 — Compilation d'OIS

### 5.1 Récupérer OIS 1.6.0

```bash
cd ~/deps
git clone https://github.com/wgois/OIS.git
cd OIS && git checkout v1.6.0
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
sudo ldconfig
```

---

## Partie 6 — Adaptation du code source BBFx

> Travailler sur la copie locale `~/bbfx-work/`.

### 6.1 Corriger `PTHREAD_MUTEX_RECURSIVE_NP`

```bash
cd ~/bbfx-work/prog/workspace/bbfx/src/core
```

Dans [core/Mutex.cpp](../prog/workspace/bbfx/src/core/Mutex.cpp), remplacer :

```cpp
// Avant
pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);

// Après
pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
```

### 6.2 Corriger `-march=athlon` dans SConstruct

Le flag `-march=athlon` génère une erreur sur GCC moderne avec un processeur autre que Athlon.

```bash
cd ~/bbfx-work/prog/workspace/bbfx/src
```

Dans [SConstruct](../prog/workspace/bbfx/src/SConstruct), modifier la ligne :

```python
# Avant
CXXFLAGS = '-O2 -march=athlon',

# Après
CXXFLAGS = '-O2 -march=native -std=c++14',
```

Faire de même dans `~/bbfx-work/prog/workspace/ogrelua/SConstruct`.

### 6.3 Adapter les chemins dans les SConstruct

#### bbfx/src/SConstruct

```python
# Remplacer les chemins hardcodés :
workspace_dir = '/home/USER/bbfx-work/prog/workspace'   # ← adapter
lua_top       = '/usr'                                    # liblua5.1 via apt
OGRE_include  = '/usr/local/include/OGRE'
OIS_dir       = '/usr/local'                              # OIS installé dans /usr/local
```

Structure adaptée :
```python
workspace_dir = '/home/' + os.environ.get('USER', 'user') + '/bbfx-work/prog/workspace'
top_dir       = workspace_dir + '/bbfx'
OGRE_include  = '/usr/local/include/OGRE'
lua_top       = '/usr'
lua_include   = lua_top + '/include/lua5.1'
lua_lib       = lua_top + '/lib/x86_64-linux-gnu'
OIS_dir       = '/usr/local'
OIS_include   = OIS_dir + '/include/OIS'
OIS_lib       = OIS_dir + '/lib'
OgreLua_top   = workspace_dir + '/ogrelua'
OgreLua_include = OgreLua_top + '/src'
OgreLua_lib   = OgreLua_top + '/lib'
```

#### ogrelua/SConstruct

```python
workspace_dir = '/home/' + os.environ.get('USER', 'user') + '/bbfx-work/prog/workspace'
OGRE_include  = '/usr/local/include/OGRE'
lua_top       = '/usr'
lua_include   = lua_top + '/include/lua5.1'
lua_lib       = lua_top + '/lib/x86_64-linux-gnu'
```

### 6.4 Corriger la syntaxe Python 3 dans les SConstruct

Vérifier s'il y a des `print` sans parenthèses (Python 2) :
```bash
grep -n "^print " ~/bbfx-work/prog/workspace/bbfx/src/SConstruct
grep -n "^print " ~/bbfx-work/prog/workspace/ogrelua/SConstruct
```

Remplacer tout `print "..."` par `print("...")` si trouvé.

### 6.5 Adapter config.lua

```bash
cd ~/bbfx-work/prog/workspace/bbfx/src
```

Dans [config.lua](../prog/workspace/bbfx/src/config.lua), adapter les chemins de resources OGRE et plugins. Remplacer les chemins absolus par les chemins WSL2 correspondants. Exemple :

```lua
-- Avant (chemins Linux 2006)
package.path  = "/common/prog/eclipse/workspace/bbfx/src/lua/?.lua;..."
package.cpath = "/common/prog/eclipse/workspace/bbfx/src/?.so;..."

-- Après (chemins WSL2 2026)
local workspace = os.getenv("HOME") .. "/bbfx-work/prog/workspace"
package.path  = workspace .. "/bbfx/src/lua/?.lua;"
             .. workspace .. "/bbfx/src/lua/lib/?.lua;"
             .. workspace .. "/bbfx/src/lua/plugins/?.lua;"
             .. package.path
package.cpath = workspace .. "/bbfx/src/?.so;" .. package.cpath
```

Pour le rendu OpenGL sous WSL2, `config.lua` devrait déjà fonctionner tel quel si OGRE est correctement installé.

---

## Partie 7 — Compilation d'OgreLua

### 7.1 Régénérer les bindings SWIG

```bash
cd ~/bbfx-work/prog/workspace/ogrelua

# Vérifier le fichier .i principal
head -20 swig/Ogre.i

# Régénérer avec SWIG 4.x
swig -c++ -lua -v \
     -DSWIGLUA_IGNORE_SET_IMMUTABLE \
     -I/usr/local/include/OGRE \
     -I/usr/include/lua5.1 \
     swig/Ogre.i 2>&1 | tee swig_ogre.log

# Vérifier les erreurs (warnings = normal, errors = problème)
grep -E "^Error|^Fatal" swig_ogre.log
```

### 7.2 Compiler OgreLua

```bash
cd ~/bbfx-work/prog/workspace/ogrelua
scons 2>&1 | tee build_ogrelua.log

# En cas d'erreur, vérifier build_ogrelua.log
tail -50 build_ogrelua.log
```

---

## Partie 8 — Compilation de BBFx

### 8.1 Régénérer les bindings bbfx

```bash
cd ~/bbfx-work/prog/workspace/bbfx/src

# Régénérer swig/bbfx_wrap.cc
swig -c++ -lua -v \
     -DSWIGLUA_IGNORE_SET_IMMUTABLE \
     -I/usr/local/include/OGRE \
     -I/usr/include/lua5.1 \
     -I. -I./core -I./input -I./fx \
     swig/bbfx.i 2>&1 | tee swig_bbfx.log

grep -E "^Error|^Fatal" swig_bbfx.log
```

### 8.2 Compiler

```bash
cd ~/bbfx-work/prog/workspace/bbfx/src
scons 2>&1 | tee build_bbfx.log

# Vérifier le résultat
ls -la libbbfx*.so 2>/dev/null || ls -la swig/libbbfx*.so 2>/dev/null
```

---

## Partie 9 — Test de l'exécution

### 9.1 Vérification des librairies dynamiques

```bash
cd ~/bbfx-work/prog/workspace/bbfx/src

# Vérifier que libbbfx_wrap.so est loadable
ldd libbbfx_wrap.so 2>/dev/null || ldd swig/libbbfx_wrap.so

# Si des librairies sont manquantes (=> not found)
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### 9.2 Configurer plugins.cfg pour WSL2

Vérifier et adapter `prog/workspace/bbfx/src/plugins.cfg` :

```ini
PluginFolder=/usr/local/lib/OGRE

Plugin=RenderSystem_GL
Plugin=Plugin_OctreeSceneManager
Plugin=Plugin_ParticleFX
```

### 9.3 Lancer BBFx

```bash
cd ~/bbfx-work/prog/workspace/bbfx/src

# S'assurer que DISPLAY est défini (WSLg)
echo $DISPLAY  # devrait afficher :0 ou similaire

# Lancer avec lua5.1
lua5.1 bbfx.lua 2>&1 | tee bbfx_run.log
```

Une fenêtre OGRE 800×800 devrait s'ouvrir avec la tête d'ogre (`ogrehead.mesh`).

### 9.4 Débogage des problèmes courants

**Fenêtre ne s'ouvre pas :**
```bash
# Vérifier DISPLAY
export DISPLAY=:0
# Relancer
lua5.1 bbfx.lua
```

**Erreur "Cannot find RenderSystem_GL" :**
```bash
# Vérifier que les plugins OGRE sont compilés
ls /usr/local/lib/OGRE/
# Adapter plugins.cfg si le chemin est différent
```

**Erreur "module bbfx not found" :**
```bash
# Vérifier package.cpath dans config.lua
# Vérifier que libbbfx_wrap.so est dans le bon répertoire
```

**Crash SWIG / erreur de type :**
```bash
# Probablement un problème de version SWIG vs runtime Lua
# Vérifier que lua5.1 et liblua5.1-dev sont de la même version
dpkg -l | grep lua5.1
```

**OpenGL : "Could not create OpenGL context" sous WSLg :**
```bash
# Vérifier les drivers Mesa
glxinfo | grep "OpenGL renderer"
# Si "llvmpipe" : rendu logiciel (lent mais fonctionnel)
# Si GPU matériel : OK
```

---

## Partie 10 — Optionnel : Remplacement de libjsw par SDL2

Si le joystick est nécessaire, remplacer `Joystick.h/cpp` par une implémentation SDL2. Voici le squelette de remplacement :

### 10.1 Créer `input/JoystickSDL2.cpp`

```cpp
// Remplacement minimal de libjsw par SDL2
#include "Joystick.h"
#include <SDL2/SDL.h>

namespace bbfx {

// Initialisation SDL2 (à appeler une seule fois)
static bool sSDL2Initialized = false;

static void initSDL2() {
    if (!sSDL2Initialized) {
        SDL_Init(SDL_INIT_JOYSTICK);
        sSDL2Initialized = true;
    }
}

// Détection des joysticks disponibles
// Retourne une table Lua avec les noms des joysticks
lua_Object Joystick::detect(const string& calibration) {
    initSDL2();
    // TODO: construire et retourner la table Lua
    int num = SDL_NumJoysticks();
    // ... (voir docs/revival-roadmap.md pour l'implémentation complète)
    return 0; // placeholder
}

// Création d'un joystick
InputDevice* Joystick::create(const string& device,
                               const string& calibration,
                               const string& name) {
    initSDL2();
    // Ouvrir le joystick SDL2
    // device format: "/dev/js0" → index 0
    int index = 0;
    // TODO: parser l'index depuis device
    SDL_Joystick* js = SDL_JoystickOpen(index);
    // ... créer et retourner un Joystick BBFx wrappant SDL2
    return nullptr; // placeholder
}

} // namespace bbfx
```

### 10.2 Adapter SConstruct

Ajouter `SDL2` aux dépendances dans `SConstruct` :
```python
libs = ['lua', 'pthread', 'OgreMain', 'X11', 'OIS', 'SDL2']
# Supprimer 'jsw'
```

---

## Récapitulatif des commandes principales

```bash
# 1. Installer toutes les dépendances
sudo apt install -y build-essential cmake python3 python3-pip swig \
    libboost-all-dev libx11-dev libgl1-mesa-dev libglu1-mesa-dev \
    libglew-dev libfreetype-dev libzzip-dev libfreeimage-dev \
    libsdl2-dev liblua5.1-dev lua5.1 && pip3 install scons

# 2. Compiler OGRE 1.4.9
cd ~/deps && wget [URL OGRE 1.4.9] -O ogre.tar.bz2 && tar xjf ogre.tar.bz2
cd ogre-v1-4-9 && mkdir build && cd build
cmake .. -DCMAKE_CXX_STANDARD=14 -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DOGRE_BUILD_SAMPLES=FALSE
make -j$(nproc) && sudo make install && sudo ldconfig

# 3. Compiler OIS
git clone https://github.com/wgois/OIS && cd OIS && git checkout v1.6.0
mkdir build && cd build && cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc) && sudo make install && sudo ldconfig

# 4. Adapter et compiler BBFx
cd ~/bbfx-work/prog/workspace/bbfx/src
# [Appliquer les modifications décrites dans les sections 6.x]
scons

# 5. Lancer
lua5.1 bbfx.lua
```

---

## Environnement de travail recommandé

| Outil | Recommandation |
|---|---|
| Terminal | Windows Terminal avec profil Ubuntu WSL2 |
| Éditeur | VS Code avec extension "Remote - WSL" |
| Débogueur C++ | gdb (dans WSL2) + VS Code GDB extension |
| Débogueur Lua | ZeroBrane Studio (Windows, se connecte via remdebug) |
| Profiler | gprof ou perf (WSL2) |
| Git | git dans WSL2 ou GitHub Desktop Windows |

---

*Document établi en mars 2026 pour la remise en fonctionnement de BBFx sur Windows 10.*
