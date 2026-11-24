# Changelog

## Unreleased

- The columns of Torrents grow with the font and are never narrower than their headers, which
  have the padding of the other lists. With a larger font, the headers ran into each other.
- Groups of settings and details have a title over a thin line instead of a box around them, in
  Settings, the anime details, Statistics and the Profile page.
- In the details of an anime, titles and tags that wrap onto more lines keep all of them. With a
  larger font or a narrower window, the next row was drawn over their last line.
- A folder made in the library folders while Akyuu runs is watched as well, so episodes put in it
  afterwards show as available without a scan. Before, only the first file that made the folder
  was noticed.
- The cards of the list are as round as the other cards and buttons of the window.
- Dates known only by month or year, as for anime that have not aired, show as "Oct 2027" or
  "2027" instead of nothing.
- The profile button opens a Profile page, which the statistics stay apart from. It shows the
  account with its avatar and banner, the episodes and time watched this week, the days in a row,
  the anime completed this year, what is airing with a countdown, what is upcoming, the available
  episodes in library folders, and what was recently watched. Airing is a schedule by day, with
  the episode and the time, Upcoming a timeline by the date each starts, and Recently Watched one
  by the day. Airing and Upcoming include the Planning list, and show only when something is
  airing or upcoming. Each row has a small poster and opens
  the details of its anime. A long list shows its first rows and opens to the rest, and only the
  rows on the screen load their posters. The sections fill two columns evenly. The library is scanned when the page is first shown.
  The account is read again on the first sync of each session, which brings its avatar and banner
  (MyAnimeList has no banner), and a renamed account no longer keeps its old name. The pictures
  are kept on disk and checked once a day.
- Torrents can be sent to qBittorrent through its Web UI, in Settings > Torrents > Downloads.
  This works when qBittorrent runs without a window, such as `qbittorrent-nox`, or on another
  computer, and the download folder and an optional category always reach it. Test checks the
  address and the sign-in before saving. The password is kept with the other accounts.
- Clapper, Dragon Player, GNOME Videos (Totem), MPlayer, Parole and Showtime are detected on
  Linux. Showtime runs as Python, so a player started from a script is now known by the script.
- The Discord activity card is headed with the title being watched, such as "Watching
  Re:CREATORS", instead of "Watching Akyuu" (#14). Thanks to @Recehan-Slayer for pointing it
  out.
- The menus share one row with the search box and the buttons, which had a row of their own with
  only the synchronize button on its left, so the page starts higher up. The search box narrows
  in a small window instead of pushing the buttons out of sight.
- The progress bar of the anime list is larger and carries the episode count inside it, as the
  cards do, instead of a thin bar with the count next to it. It has rounded corners, as does the
  search box.
- The tabs above the anime list show their counts in a small badge, like the sidebar, instead
  of in parentheses.
- The sidebar follows the size of the application font, with 10 points at least. It was fixed
  at 10 points, so it looked small next to the rest of the window with a larger font.
- Episodes named like `Title - 07 (S01E07)`, as some groups do, are recognized. The number
  before the season and episode was read as part of the title, so the anime was not found.
- More file names are read right, and recognized: episode 1080 of One Piece is not taken for a
  resolution, `EAC-3` and `AC-3` are audio rather than episode 3, a checksum such as `ED204890`
  is not an ending, and a release group such as `Y-467` does not give the episode number.
- Files with an uppercase extension, such as `.MOV` from a phone or `.MKV`, are recognized.
  They were taken for something other than a video, so playing them went unnoticed and library
  scans skipped them.
- Opening a completed title that is being rewatched no longer sets its progress to 0, which
  was saved with OK. This happened with MyAnimeList and Kitsu, where such a title stays completed.
- List updates keep being sent when MyAnimeList or Kitsu cannot renew an expired sign-in. The
  update waiting for it was never finished, so the ones after it stayed in History until Akyuu
  was restarted. An update the service keeps refusing no longer holds back the others either.
  Thanks to @SGalazzo, who found both in Taiga.
- A Kitsu password and MyAnimeList notes keep their plus signs and percent signs. A plus sign
  became a space, so a Kitsu password with one could not sign in.
- Changing the update delay, its trigger or the confirmation in Settings applies to the episode
  that is playing. They applied from the next episode on. The time already watched still counts.
- Anime List in the sidebar no longer stays highlighted after another page is picked, and the
  page that is showing stays marked after a sync or a list update.
- The Type and Last updated columns of the anime list widen with a larger font or zoom, so
  "Special" and "7 months ago" are no longer cut short.
- The Title column of the anime list can be dragged wider or narrower; it filled the rest of the
  row and could not be resized. Dragging an edge moves it between its two columns only, and the
  widths are kept the next time.
- The names and values in Settings > Advanced keep apart, and its rows are easier to tell apart.
- The shortcut letters of menus are underlined only while Alt is held, as in Plasma, instead of
  all the time. List and Library have one now (Alt+L, Alt+B), and Services has S, as View had V
  as well.
- The values on the Statistics page start at the same place in every group, so the page reads as
  one table.
- The pages of Settings scroll, so the window opens smaller and can be made shorter. With a large
  font it could not be made shorter than its longest page, which took up most of the screen.
- Icons are drawn once on a scaled display, such as at 125% or 160%, instead of on every repaint.

## 0.5.0

- Seasons and Search can require or exclude types of titles as well as genres: TV, OVA, movie,
  special, ONA and music (#13). Thanks to @stefanostrowiak for the idea.
- Discord Rich Presence works on Windows and with the Vesktop Flatpak (#14). Thanks to
  @Recehan-Slayer for reporting it.
- Discord shows the anime as "Watching" with its title under your name, instead of "Playing
  Akyuu" (#14).
- A pressed menu choice no longer shows a second check mark, damaged genre settings are
  ignored, ampersands stay in genre chips, and the About dialog fits on a laptop screen.

## 0.4.0

- Settings > Application > Content decides what Seasons and Search show (#13). Adult titles
  can be shown, blurred until clicked, or hidden, and the View menu of both pages has the same
  choice. Each genre can be required or excluded, from the same tab or from the new Genres
  button, whose label counts the rules in use. Seasons and Search say how many titles were
  hidden. The settings hold the defaults; the Genres button and the View menu change them until
  Akyuu is closed, marked with a dot and a way back to the default.
- The title count of Seasons follows the search box.
- Switching Search or Seasons between the list and the cards no longer crashes Akyuu the next
  time the search box changes.
- Cards no longer show the title tooltip, which repeated what the card says and could open and
  close over and over under the cursor on Wayland.
- Settings: 0.3.0's View > Hide adult titles is read once. Whoever turned it off starts with
  adult titles shown; otherwise they stay hidden, which is the default.
- Known limitation: a signed in AniList or Kitsu account receives adult titles only when the
  account allows 18+ content, so Show and Blur have nothing to show otherwise.

## 0.3.0

- Seasons hides adult titles again, as Taiga did, and View > Hide adult titles shows them when
  turned off (#12). Thanks to @jonathancerqueira for asking for it.
- The availability bar shows which episodes are in the library folders (#11). It only counted
  the files, so a gap in the middle of a series appeared at the end. Thanks to
  @stefanostrowiak for reporting it.
- A file that holds several episodes, such as `01-02`, counts for all of them. Only the last one
  was marked as available.
- Media control on Linux reads the session bus with its own small D-Bus client, so Akyuu no
  longer needs libsystemd, elogind or basu, and one package works with or without systemd.
  `-DANISTHESIA_SD_BUS_PROVIDER` is gone.
- The owl's tune on Linux is played with `pw-play`, `paplay` or `aplay`, so Qt Multimedia and
  the media backend it brings along are no longer needed.

## 0.2.1

- Akyuu builds on systems without systemd, such as Artix, with sd-bus from elogind or basu
  (#9). Media control on Linux needs sd-bus, which was looked for in systemd only, so the build
  failed unless media control was turned off. Packagers can pick the provider with
  `-DANISTHESIA_SD_BUS_PROVIDER`. Thanks to @shingym00 for reporting it.
- Media played from a USB stick or another removable drive is detected on Linux. Desktops
  mount these drives under `/run/media`, which was skipped along with the rest of `/run`.
- Detection on Linux takes about a third less time, as the player list is prepared once per
  check and only the processes of known players are read further.
- Keep long AniList retry delays from overflowing into an immediate retry.
- Keep the saved detection interval between one and sixty seconds, as in Settings.
  A zero or negative value could make detection run continuously.
- The search filters can shrink in a narrow window or with a large font, so they do not
  squeeze the sidebar down to its icons. Their dropdowns keep the full labels readable.

## 0.2.0

- The search box says "Filter list" on the page Akyuu opens with. It said "Search", so typing a
  title there and pressing Enter filtered the list instead of searching the service, and found
  nothing on a new installation. Thanks to Arlen for finding it.
- The Now Playing panel hides a few seconds after the media is closed and the list is updated
  (#6). A question that is still waiting for an answer stays. Settings > Recognition can keep it
  until the next media as before, or show it only to ask for confirmation. Thanks to
  @stefanostrowiak for asking for it.
- A poster whose download fails, for example on a dropped connection, is tried again a minute
  later. It stayed empty until the page was opened again or the media changed.
- The sidebar can be dragged wider, up to one and a half times the width of its labels, and
  keeps that width the next time. It was held at 200 pixels.
- The System style follows a picked Light or Dark color scheme with styles that come in a dark
  variant, such as Adwaita-Dark behind qt6ct, by using the other variant directly. Boxes such as
  the search filters stayed dark on a light window. The style is then used without the proxy
  that qt6ct puts in front of it, so qt6ct's own adjustments do not apply to it.
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
