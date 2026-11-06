# ROM Downloader 3.2.1

A Nintendo Switch homebrew downloader with a pixel-font retro interface, live console palettes, and a background download queue. The interface was designed specifically for this app.

Music and eBook share the Media tab and are excluded from Consoles. L/R switches tabs on the systems screen; each tab remembers its selection during the session. Queues can still include files from different tabs.

## Two interfaces

**Full RAM:** twelve system tiles per page in a 4-column, 3-row grid, retaining the large 248 x 120 artwork areas. Titles and file counts overlay the artwork, cached artwork or pixel cartridge/handheld icons, a file list with queue/destination details, and a transfer dashboard.

**Applet:** a full-width collection list, full-width file selection and transfer screens, no cover textures, an 80-entry text cache (220 in full mode), and a 4 MiB catalog response limit (24 MiB in full mode). Both modes support the same download workflow. Layout is selected automatically from the libnx applet type.

![Full RAM shelf](docs/full-ram.png)
![Applet collection list](docs/applet.png)

## What's new

- Twelve live console palettes, animated palette transitions, sliding screen entrances, aligned selection markers, scrolling long filenames, and segmented download bars. Reduced motion is available.
- VT323 pixel typography with solid glyph rendering, clipped text, selection counts, contextual controls, visible scrollbars, and clear empty/error states.
- Search within a collection, favorites, favorites filtering, alphabetical sorting, page navigation, select-visible/deselect-visible, and selections retained across collections.
- Review the whole queue before starting; remove individual items, choose an SD folder, create folders, or pick from the last 12 destinations.
- Background catalog loading and downloads. No artificial loading delay. Cached catalogs work offline; a failed update retains the usable cached catalog.
- Download percentage, byte counts, transfer speed, estimated time remaining, pause/resume, confirmed cancellation, per-file results, and retry for failed/cancelled downloads.
- Files download to unique temporary paths and are committed after success. Existing files are skipped. Cancellation removes the active partial file and keeps completed files.
- Optional ZIP extraction with progress, pause/cancel, entry-path validation, CRC/size checking, and preservation of existing files. Choose Off, Extract + keep ZIP, or Extract + delete ZIP in Settings or with Y on the review screen. Delete mode removes the original only after every file has been successfully extracted and verified. Errors, cancellation, empty archives, or skipped existing files keep the ZIP. Arcade archives remain packed and are never deleted.
- Preferences, favorites, and recent paths persist. The previous settings/history/cache files are read for migration; new preferences live in `sdmc:/switch/ROM_Downloader/studio.json`.

## Controls

| Screen | Controls |
| --- | --- |
| Everywhere | +: settings while browsing. Change themes in Settings. |
| Systems | D-pad / left stick: navigate. A: open. L/R: Consoles / Media tabs. Y: refresh. +: settings. |
| Files | A: toggle selection. X: review downloads (uses focused file if nothing is selected). Y: search (clear the query for all files). L: favorites filter. R: sort direction. R-stick click: favorite. Minus: select/deselect visible results. ZL/ZR: page. B: shelf. |
| Folder | A: open. L: parent. R: create folder. Y: recent paths. X: use folder and return to queue. B: queue. |
| Recent paths | A: use. B: folder browser. |
| Review | A: start. X: remove focused item. Y: cycle extraction mode (off / keep ZIP / delete ZIP). R: destination. B: files. |
| Download / extraction | A: pause/resume. B: stop confirmation; A confirms, B returns to the previous pause state. |
| Results | A/B: files. X: retry failed/cancelled downloads. |
| Settings | A: change/activate. Left/Right: cycle the focused palette. B: back, preserving the prior cursor. Exit is the last row. |

Workflow: open a system, press X on a file (or use A to select several), then A on the queue to download. The queue uses the current destination; press R there to choose another folder.

Selections can span collections. All selected files use the chosen destination. If two selected items have the same output filename, the first successful save wins and the later file is skipped. No existing file is replaced.

## Build

Use devkitPro/devkitA64, libnx, Switch SDL2/SDL2_image/SDL2_ttf/SDL2_mixer, curl, and minizip portlibs. The existing audio/image dependencies remain listed in `Makefile`.

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITA64=$DEVKITPRO/devkitA64
make -j2
```

Copy `ROM_Downloader.nro` to your Switch homebrew folder. RomFS embeds fonts and sounds.

## Code map

| File | Responsibility |
| --- | --- |
| `source/main.cpp` | Platform initialization, frame loop, shutdown |
| `include/app.hpp` | Screens, preferences, queue, and shared application interfaces |
| `source/model.cpp` / `include/model.hpp` | Portable catalog parser, filtering, path validation, formatting |
| `source/input.cpp` | Controller mappings and workflow transitions |
| `source/storage.cpp` | Preferences/migration, SD folder browser, keyboard, destination history |
| `source/worker.cpp` | Background catalog/download worker, ZIP extraction, synchronized progress |
| `source/ui.cpp` | Retro layouts, screen rendering, bounded text/artwork caches, animation |
| `source/themes.h` | Twelve existing console palettes |

The worker owns its task copies and never touches SDL textures or live UI state. It publishes progress through a mutex and uses atomic pause/cancel flags. The UI thread joins finished workers before consuming their results.

## Tests and previews

`tests/model_tests.cpp` exercises catalog JSON, malformed records, duplicate filenames, search/favorites/sorting, SD paths, archive paths, and large byte sizes. Build with a desktop C++17 compiler:

```sh
c++ -std=c++17 -Iinclude tests/model_tests.cpp source/model.cpp -o model_tests
./model_tests
```

`tests/desktop_preview.cpp` links the production renderer and input module with desktop SDL2, SDL2_ttf, SDL2_image, SDL2_mixer and `tests/stubs/switch.h`. It uses synthetic catalog records, tests selection/search/favorites/queue/pause/cancel, renders all 12 themes, and writes PNGs under `build/`. It does not contact download servers. `tests/run-desktop.ps1` documents and automates the Windows version of these checks with a locally supplied Zig compiler and SDL DLLs.

The screenshots above come from that actual renderer, using synthetic data. Desktop tests do not replace Switch testing: check handheld/docked text, applet memory, keyboard return, long downloads, SD-full errors, ZIP failures, pause over 40 seconds, sleep/wake, and shutdown on hardware.

## Limits

- Pause resumes the current session; downloads do not resume across app restarts.
- ZIP extraction supports the installed minizip decoder. Password-protected archives, links, duplicate entry paths, unsafe paths, and archives above 20,000 entries are rejected. Other archive types are kept as downloads. Extraction warnings are shown in the saved file's result detail; retry applies to downloads, not extraction.
- Partial extraction can leave successfully completed extracted files; cancellation removes only the active temporary file. Existing extracted files are skipped.
- Cover art uses previously cached images in `covers/`; uncached collections get procedural cartridge labels. New artwork is not fetched during catalog startup.
- The catalog endpoint remains the project's existing endpoint. Downloads follow HTTPS redirects and retain certificate verification.

## Third-party notices

JSON parsing uses nlohmann/json; its MIT license is in `licenses/nlohmann-json-MIT.txt`. Atkinson Hyperlegible comes from the [Google Fonts distribution](https://github.com/google/fonts/tree/main/ofl/atkinsonhyperlegible); its SIL Open Font License is in `licenses/AtkinsonHyperlegible-OFL.txt`. Existing project fonts, sounds, and icon are retained.

The pixel UI uses [VT323 from Google Fonts](https://github.com/google/fonts/tree/main/ofl/vt323), under the SIL Open Font License included at `licenses/VT323-OFL.txt`. Earlier fonts remain in RomFS for compatibility but are not used by this interface.
