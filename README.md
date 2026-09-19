# Mirage
**Mirage** is a standalone server host executable for SM64CoopDX. It's more optimized than using the game's headless mode due to it only handling networking and not running any of the game logic which frees up a lot of work for your device due to it not needing to run the game.

---
**Mirage** is currently still a work in progress but it's functional in its current state and it supports CoopNet and Direct Connect as network types (Direct Connect requires you to port forward)

# Building

## 1. Install dependencies
### Ubuntu / Debian
```bash
sudo apt install build-essential clang libz-dev liblua5.3-dev
```

### Arch Linux
```bash
sudo pacman -S base-devel clang zlib lua
```

### Fedora
```bash
sudo dnf install make clang zlib-devel lua-devel
```

### Windows (MinGW)
```bash
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-zlib mingw-w64-x86_64-lua
```

## 2. Clone the Repository

```bash
git clone https://github.com/ManIsCat2/Mirage
cd Mirage
```

## 3. Build
```bash
make -j$(nproc)
```

The compiled binary will be saved in the `build/` directory.

# Running
Run the program when you're in the Mirage folder:
```bash
./build/Mirage
```
Or on Windows:
```bash
./build/Mirage.exe
```
Note: Running it for the first time generates a JSON configuration file. Edit this file and restart Mirage to apply your  settings.