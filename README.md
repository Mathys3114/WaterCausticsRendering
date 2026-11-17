# WaterCausticsRendering

Base de projet pour un rendu de caustiques aquatiques en temps reel avec C++ et OpenGL. Le developpement et la compilation sont prevus sous Linux ; aucun compilateur C++ n'est necessaire sur Windows pour preparer le depot.

## Structure

```text
assets/                  Ressources (textures, modeles...)
include/water_caustics/  Interfaces C++ du projet
shaders/                 Shaders GLSL
  quad.vert              Vertex shader du plan maillé
  quad.frag              Fragment shader du plan maillé
src/
  main.cpp               Point d'entree et boucle de fenetrage
  render/                Rendu et gestion OpenGL
  simulation/            Simulation de l'eau et des caustiques
tests/                   Tests (a ajouter avec les premiers modules)
CMakeLists.txt           Configuration et cible de compilation
vcpkg.json               Dependances C++ gerees par vcpkg
```

Les bibliotheques ne sont pas copiees dans le depot : vcpkg les telecharge et les construit sur la machine cible. CMake utilise la cible OpenGL portable `OpenGL::GL`, sans chemin de bibliotheque propre a Windows.

## Prerequis Linux

Exemple pour Ubuntu ou Debian :

```bash
sudo apt update
sudo apt install build-essential cmake git pkg-config libgl1-mesa-dev xorg-dev autoconf autoconf-archive automake libtool
```

Installer ensuite vcpkg :

```bash
git clone https://github.com/microsoft/vcpkg.git ~/dev/vcpkg
~/dev/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/dev/vcpkg"
```

`export` ne s'applique qu'au terminal courant. Pour le conserver, ajouter cette variable a la configuration du shell ou utiliser le chemin complet vers `vcpkg.cmake`.

## Configurer et compiler sous Linux

Depuis la racine du depot, configurez et compilez depuis un dossier de build :

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --parallel
./WaterCausticsRendering
```

Avec `VCPKG_ROOT` defini vers une installation de vcpkg, CMake charge automatiquement son toolchain. Si le dossier `build` a deja ete configure sans ce toolchain, utilisez un nouveau dossier de build (par exemple `build-vcpkg`) pour cette premiere configuration.

La premiere configuration telecharge et construit les dependances declarees dans `vcpkg.json` et necessite une connexion Internet. L'application affiche une vague sinusoidale animee et translucide au-dessus d'un plan de sol, vue en perspective par une camera fixe ; le calcul des caustiques reste a implementer.

## Dependances

- GLFW : fenetre, contexte OpenGL et entrees
- GLAD : chargement des fonctions OpenGL 3.3
- GLM : types et operations mathematiques pour le rendu 3D
