# Changelog

## 0.1.0-beta.3

- Recognize Firefox, Chromium and Zen media titles without a page suffix. Browser titles without
  an episode number no longer become unknown anime, while known single-episode films remain
  eligible. A disabled provider still prevents ambiguous browser updates.
- Share release selection and downloads across platforms. Downloads match the system, processor
  and installation format, and are saved only after size and SHA-256 verification. Interrupted
  or invalid downloads preserve any existing destination file.
- Supply an x86_64 AppImage with Qt and compiler runtime libraries for other Linux distributions.
  The same package was checked in Ubuntu 24.04 and Fedora 44 containers without Qt installed;
  this covers startup and HTTPS, not every desktop integration or graphics driver.

Installation remains with the user or package manager. Windows and macOS binaries are not
provided. Multiple MPRIS sessions, paused countdowns, Flatpak detection and keyring storage
remain open, as described below.

## 0.1.0-beta.2

- Torrent notifications now follow the filtered selection, as in Taiga v1. Unselected or
  discarded files no longer trigger a notification just because their episode number is ahead
  of your progress.
- Installation instructions are in the wiki's existing build guide; the README stays concise.

The known limitations listed for beta.1 still apply.

## 0.1.0-beta.1

First Akyuu beta for Linux, based on Taiga's Qt rewrite. This release retains the upstream
history and licenses. It is intended for daily use and feedback while the remaining rough
edges are worked through.

### Included

- Anime lists and progress synchronization with AniList, Kitsu and MyAnimeList.
- Native Linux media detection through Anisthesia, MPRIS and focused-window information.
- Library management, seasonal browsing, torrent feeds and filters.
- Discord, IRC and HTTP sharing, list shortcuts, themes and tray operation.
- Numbered beta releases, a beta-aware update check and a local privacy notice before
  MyAnimeList authorization.

### Fixed

- Opening an episode with no configured player could recurse and crash. Existing library files
  now also respect the selected player.
- Leaving through the tray now saves window and list layout settings.
- Changing between unknown titles or different files for the same episode now resets the
  appropriate tracking state.
- Nested RSS fields no longer cause an otherwise readable feed to be discarded.
- Format replacement no longer repeats indefinitely when replacement text contains the search
  text.
- Account files on Unix use owner-only permissions. Debug output redacts session cookies.
- Detection handles player processes left running after their executable was replaced.

### Known limitations

- Multiple MPRIS sessions belonging to one process can mix title and URL information.
- Pausing playback does not pause the progress update countdown; focus restrictions are separate.
- Flatpak process identification and Chromium-family browser metadata are limited. Browser
  detection is disabled by default.
- Stored credentials are not encrypted; operating-system keyring integration is pending.
- This release targets Linux. Windows and clean Arch chroot builds have not been validated.

Back up important local data before testing migrations. When reporting a problem, include the
version, player, triggering steps and expected behavior. Remove credentials, account details
and private paths from attachments.
