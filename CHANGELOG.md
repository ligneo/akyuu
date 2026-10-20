# Changelog

## Unreleased

- The search box says "Filter list" on the page Akyuu opens with. It said "Search", so typing a
  title there and pressing Enter filtered the list instead of searching the service, and found
  nothing on a new installation. Thanks to Arlen for finding it.
- The Now Playing panel hides a few seconds after the media is closed and the list is updated
  (#6). A question that is still waiting for an answer stays. Settings > Recognition can keep it
  until the next media as before, or show it only to ask for confirmation.
- A poster whose download fails, for example on a dropped connection, is tried again a minute
  later. It stayed empty until the page was opened again or the media changed.
- The sidebar can be dragged wider, up to one and a half times the width of its labels, and
  keeps that width the next time. It was held at 200 pixels.
- The System style follows a picked Light or Dark color scheme with styles that come in a dark
  variant, such as Adwaita-Dark behind qt6ct, by using the other variant directly. Boxes such as
  the search filters stayed dark on a light window.
- The search page searches the service a moment after typing stops at three characters or more,
  not only on Enter. Typing without Enter filtered the results already on the page, and found
  nothing on a new installation.
- AniList requests that hit the rate limit wait as long as AniList asks and are sent again,
  instead of failing.
- Torrent feeds have 30 seconds to answer, up from 10. Slow feeds failed now and then with
  "Operation timed out".
- An episode whose title cannot be read says so in its notification and on Now Playing, which
  were blank.
- The update delay waits while the player says it is paused, whether or not "Pause delay while
  player is not in focus" is on. An episode opened and left paused counted as watched.
- While the anime list is filtered, its tabs count the matches, so a title under another tab shows
  where it is.

## 0.1.1

- Build with GCC 15 and Qt 6.10, so that Fedora, openSUSE, Ubuntu and Debian can build Akyuu
  from source.
- Packages from a distribution's repository leave updates to its package manager.

## 0.1.0

- The first stable release. It contains the same changes as 0.1.0-beta.6; only the version
  changes. Beta users get it as an update, and stable users are not offered betas.

## 0.1.0-beta.6

- The statistics page no longer squeezes its groups in a short window (#1). Its text shrinks to
  fit, down to 80%, and the page scrolls below that. The sidebar narrows instead of covering the
  page on tiling window managers that ignore the minimum window size.
- Every page zooms with Ctrl+Shift and +, - or 0, or with Ctrl and the mouse wheel, on any
  keyboard layout. Each page keeps its own level; rows, cards, headers and posters grow with the
  text. Ctrl with +/- still changes the episode in the anime list.
- Add a light variant of the Akyuu style, picked with the Light color scheme. The color scheme
  applies to every style, and icons repaint when the palette changes.
- On Now Playing, History and Statistics, typing in the search box and pressing Enter searches
  the service.
- Sidebar pages change on a click, not while dragging over them.
- Keep media sessions of the same player apart, pause the update delay while the player is
  paused, and query media players in parallel.
- Keep tray notifications in the notification history.
- Build packages in GitHub Actions when a release is tagged, and build Akyuu with Nix.

## 0.1.0-beta.5

- Supply Debian/Ubuntu and Fedora/openSUSE packages alongside the AppImage and Arch package.
  AppImage, DEB and RPM builds share a private Qt and compiler runtime; Arch uses
  distribution libraries. Each format has its own update target.
- Supply a Windows x64 installer with Qt and the Microsoft Visual C++ runtime. Uninstalling
  removes installed application files while preserving user data and unrelated files.
- Build packages in GitHub Actions, test installed runtimes, and prepare a checked release draft
  after the platform checks pass. Publication remains a maintainer action.
- Include libraries omitted by desktop deployment defaults and collect the corresponding
  versioned third-party sources and licenses for every bundled Linux library. Check the
  produced source archive inventories, notices and checksums before creating a release draft.

macOS remains deferred. Linux desktop and media detection checks remain separate from package
installation checks; the earlier MPRIS and credential-storage limitations still apply.

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
