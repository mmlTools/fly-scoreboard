# Fly Scoreboard templates

The templates in this directory are starter files. Copy the complete `Modular Football` directory to a location that your user account and OBS Studio can both read and write before selecting it in the Fly Scoreboard dock.

Fly Scoreboard writes live state to `plugin.json` and may copy team logos into the selected template directory. Do not run a template directly from `Program Files`, an application bundle, `/usr`, or another protected plugin-install directory.

Recommended writable locations:

- Windows: `%USERPROFILE%\Documents\Fly Scoreboard\Templates\Modular Football`
- macOS: `~/Library/Application Support/Fly Scoreboard/Templates/Modular Football`
- Linux (native package): `${XDG_DATA_HOME:-$HOME/.local/share}/fly-scoreboard/templates/Modular Football`
- Linux (OBS Flatpak): `~/.var/app/com.obsproject.Studio/data/fly-scoreboard/templates/Modular Football`

Copy every file in the template directory, including all HTML files, `style.css`, `script.js`, `manifest.ini`, and `plugin.json`. In OBS, add each HTML panel you want as its own local-file Browser Source. The default package provides `index.html`, `fouls.html`, `cards.html`, and `corners.html`.

If you choose a different location, ensure OBS has filesystem permission to it. This is especially important for sandboxed installations such as Flatpak.
