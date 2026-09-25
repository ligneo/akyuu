# Akyuu

[![](https://img.shields.io/github/license/ligneo/akyuu)](LICENSE)

Akyuu is an open-source desktop anime tracker for Linux. It detects the anime videos you watch on
your computer and synchronizes your progress with [AniList](https://anilist.co),
[Kitsu](https://kitsu.app) or [MyAnimeList](https://myanimelist.net). It helps you manage your
anime library, follow the season, share watched episodes and download new ones.

> **Status:** early development (0.1.0-alpha). Things work day to day, but expect rough edges.

## Based on Taiga

Akyuu is a fork of [Taiga](https://github.com/erengy/taiga) by Eren Okka, modified since
September 2026. It started as a Linux port of Taiga's Qt rewrite and has since become its own
project:

- Media detection on Linux, through MPRIS and the focused window, for video players and browsers
- The layout and density of Taiga v1, with the features of v1 that were still missing in the
  rewrite: torrent feeds and filters, sharing over Discord, IRC and HTTP, list shortcuts, and more
- Its own name, data folder and release cycle

The full history of Taiga is kept in this repository, so the work of Eren Okka and the other
Taiga contributors stays visible commit by commit.

More on building, moving from Taiga, debug logs and contributing is in the
[wiki](https://github.com/ligneo/akyuu/wiki).

## Building

Requirements: CMake 3.21+, a C++23 compiler, and Qt 6 with the Concurrent, DBus, Network, Sql,
Svg, Widgets and LinguistTools modules. Qt Multimedia is optional.

```sh
git clone --recurse-submodules https://github.com/ligneo/akyuu.git
cd akyuu
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

This installs the `akyuu` executable, a desktop entry and an icon.

Data is kept in `~/.local/share/akyuu/data`. If you used Taiga before, the first run copies
Taiga's data folder from `~/.local/share/erengy/taiga/data`, and leaves the original untouched.

Pass `-DAKYUU_PORTABLE=ON` to keep data in `bin/data` next to the executable instead.

### Related projects

- [Anime relations](https://github.com/erengy/anime-relations) (episode redirections)
- [Anisthesia](https://github.com/ligneo/anisthesia) (media detection library, with Linux support)
- [Anitomy](https://github.com/erengy/anitomy) (anime video filename parser)

## License

Akyuu is licensed under the [GNU General Public License v3](LICENSE).

Copyright (C) 2010-2026, Eren Okka (Taiga)<br>
Copyright (C) 2026, cenky
