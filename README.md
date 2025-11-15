# 🏆 Fly Scoreboard – OBS Studio Plugin

A lightweight, **hotkey-friendly scoreboard overlay** for OBS Studio designed for live sports, esports, and streaming events.  
It runs a local web server that powers an **auto-updating browser overlay**, allowing real-time updates via dock controls or hotkeys.

[![Support on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/mmltech)

---

## 🖼️ Preview

<p align="center">
  <picture>
    <source srcset="docs/assets/img/preview.webp" type="image/webp">
    <img src="docs/assets/img/preview.png" alt="Fly Scoreboard Preview" width="800">
  </picture>
</p>

---

## ✨ Features

- 🕐 **Live Timer** - Count up or down, pause, reset, and auto-syncs with overlay.
- 🏁 **Scoreboard Dock** - Control home/guest scores, rounds, and visibility.
- 💡 **Hotkey Support** - Bind keys for +1/−1, swap teams, and show/hide.
- 🧩 **Web Overlay** - Auto-generated and served via local HTTP (`overlay/index.html`).
- 🖼️ **Logo Uploads** - Automatically copied with hashed filenames to prevent cache issues.
- 🧹 **Clear + Reset** - Wipes teams, deletes logos, and restores defaults.
- ⚙️ **Settings Dialog** - Configure web server port, health check, open overlay folder, and hotkey help.

> 💬 **Lightweight and fast:** zero dependencies, just Qt + OBS SDK.

---

## 📦 Installation

### From Release (Recommended)

1. Download the latest **Release ZIP** from [GitHub Releases](https://github.com/mmlTools/fly-scoreboard/releases).
2. Extract it into your OBS plugins directory, e.g.:
   ```
   C:\Program Files\obs-studio\obs-plugins\64bit\
   ```
3. Launch OBS → `View → Docks → Fly Scoreboard`.

### From Source (Manual Build)

#### Windows (Visual Studio 2022)

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DENABLE_FRONTEND_API=ON -DENABLE_QT=ON -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix "E:\obs-studio"
```

#### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_FRONTEND_API=ON -DENABLE_QT=ON
cmake --build build --config Release
sudo cmake --install build
```

---

## 🧠 Usage Overview

Once installed and OBS restarted:

1. Open **Dock → Fly Scoreboard**.
2. Configure team titles, logos, and scores.
3. Control the **timer** and **scoreboard visibility**.
4. Click **Apply** the overlay updates instantly in your Browser Source.

### Hotkeys

Bind keys in **OBS → Settings → Hotkeys** under the section `Fly Scoreboard:`

- `Home Score +1 / −1`
- `Guests Score +1 / −1`
- `Swap Home ↔ Guests`
- `Show / Hide Scoreboard`

---

## 🧰 Overlay Setup

The plugin automatically manages a Browser Source (name: `Fly Scoreboard Overlay`) pointing to:

```
http://127.0.0.1:<port>/overlay/index.html
```

If missing, manually add a new **Browser Source** and set that URL.

All state is stored in:

```
<config_path>/overlay/plugin.json
```

---

## ⚙️ Development

| Platform        | Toolchain / Notes                               |
| --------------- | ----------------------------------------------- |
| 🪟 Windows      | Visual Studio 17 2022 / Qt 6.x                  |
| 🍎 macOS        | Xcode 16.0 / Homebrew Qt                        |
| 🐧 Ubuntu 24.04 | CMake ≥3.28, ninja, pkg-config, build-essential |

### Build Commands

```bash
cmake -S . -B build -DENABLE_FRONTEND_API=ON -DENABLE_QT=ON
cmake --build build --config Release
```

### Run Locally

OBS will automatically detect and load `fly-scoreboard.dll` / `fly-scoreboard.so` on startup.

---

## 🧩 Project Structure

```
src/
  fly_score_dock.cpp        # Main dock UI
  fly_score_plugin.cpp      # OBS plugin entrypoints
  fly_score_server.cpp      # Embedded web server
  fly_score_state.cpp       # JSON state management
  fly_score_hotkeys.cpp     # Hotkey registration
  fly_score_settings_dialog.cpp # Settings dialog
overlay/
  index.html, style.css, script.js
```

---

## 💬 Support & Contribution

- 💖 [Support the project on Ko-fi](https://ko-fi.com/mmltech)
- 🐛 Report bugs via [GitHub Issues](https://github.com/mmlTools/fly-scoreboard/issues)
- 📘 Read the [Full Documentation →](https://mmlTools.github.io/fly-scoreboard/)

---

## 📜 License

MIT License © 2025 [MMLTech](https://github.com/MMLTech)

> Built with ❤️ using the OBS SDK, Qt, and a lot of caffeine.
