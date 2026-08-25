# Fly Scoreboard 5.0.0

Fly Scoreboard 5.0 is a major update focused on modular sports graphics, simpler control, and a clearer setup experience.

## Important changes before updating

- Fly Scoreboard no longer creates, selects, renames, resizes, or changes OBS Browser Sources.
- Existing OBS sources remain under your control. Add each HTML panel manually as a local-file Browser Source and arrange it directly in the OBS preview.
- Templates must be copied to a location where OBS can read and write files, such as your Documents folder, before use.
- Use **Template Folder...** in the dock to choose the copied template directory itself. The previous source/template selection dropdown has been removed.

## New modular football setup

- Added a complete football broadcast setup with four independent panels:
  - Scoreboard
  - Fouls
  - Yellow and red cards
  - Corners
- Every panel has its own HTML file, so it can be added, positioned, resized, shown, or hidden independently in OBS.
- All panels stay synchronized with the same teams, scores, match statistics, timers, and visibility state.
- The default football setup now includes Score, Fouls, Yellow Cards, Red Cards, and Corners controls.

## Easier template workflow

- Replaced the old dropdown workflow with one clear **Template Folder...** button.
- The active template name is shown as plain text in the dock.
- Added an **Open** button for quick access to the active template files.
- The plugin creates and updates the shared match state in the selected writable template folder.
- Included a template README with recommended writable locations for Windows, macOS, native Linux, and OBS Flatpak.

## Scores, statistics, and timers

- Scores are no longer limited to three digits.
- Added clearer controls for football-specific statistics.
- Improved left/right team handling when sides are swapped.
- Multiple timers can be created and displayed in custom templates.
- The default match timer now counts upward.
- Templates can display timers as minutes and seconds or include hours for longer events.

## Event logs and timestamps

- Added an **Events** tab to the dock.
- Streaming or recording can automatically start a new event log.
- Score changes, statistic changes, timer actions, visibility changes, and side swaps can be recorded automatically.
- Manual events such as penalties, goals, substitutions, or shots on target can be added during a match.
- Choose between stream-relative timestamps and real clock time.
- Event files can be opened directly from the dock and used for chapters, highlights, or match reports.

## Remote control and live data

- Added local remote control for scores, statistics, timers, visibility, and event logging.
- External sports-data tools can update Fly Scoreboard while manual dock controls remain available.
- Modular panels receive updates together, keeping the complete sports setup synchronized.

## Installation improvements

- Windows packages now place the plugin, locales, and templates in their correct OBS folders.
- macOS and Linux packages use their native OBS plugin locations.
- Templates are clearly separated from the plugin files and include instructions for copying them to a writable location.

## Documentation

- Reworked the documentation around the new manual-source and modular-template workflow.
- Added complete setup and installation instructions for Windows, macOS, Linux, and OBS Flatpak.
- Added a full template reference explaining available tags, scores, statistics, timers, conditional content, and reusable panel creation.
- Added guides for external live data, remote control, event logs, and timestamps.
- Redesigned the documentation website with a flat, clearer, and more direct layout.
