# Akyuu

[![](https://img.shields.io/github/license/ligneo/akyuu)](LICENSE)

Akyuu is an open-source desktop anime tracker for Linux. It detects the anime videos you watch on
your computer and synchronizes your progress with [AniList](https://anilist.co),
[Kitsu](https://kitsu.app) or [MyAnimeList](https://myanimelist.net). It helps you manage your
anime library, follow the season, share watched episodes and download new ones.

> **Status:** beta. Intended for daily use and feedback; known limitations are listed in the
> [changelog](CHANGELOG.md).

## Download and install

Open [Releases](https://github.com/ligneo/akyuu/releases) and select a published Akyuu release.
Download the appropriate file under **Assets**. The first beta is currently a draft; its files
will become available to other users when the repository and release are public.

| System | Available installation |
| --- | --- |
| Arch Linux, x86_64 | Prebuilt `.pkg.tar.zst` package |
| Other Linux distributions | Build the full-source `.tar.xz` archive |
| Windows | No tested build or `.exe` installer is available yet |
| macOS | Not supported by this beta; native media detection is not implemented |

### Arch Linux

Download `akyuu-0.1.0beta.1-1-x86_64.pkg.tar.zst` from the release assets. From the directory
containing that file, install it with:

```sh
sudo pacman -U ./akyuu-0.1.0beta.1-1-x86_64.pkg.tar.zst
```

Then launch **Akyuu** from your application menu or run `akyuu`.
The package declares its dependencies; pacman installs missing dependencies from configured
repositories. Use a fully updated Arch installation. This beta package was built with Qt 6.11.2,
GCC 16.2 and glibc 2.44 and is not intended for other distributions. AUR publication is pending.

### Other Linux distributions

Download `akyuu-0.1.0-beta.1-source.tar.xz`. It includes the exact dependency sources used by the
release. Install the build requirements listed below, then:

```sh
tar -xf akyuu-0.1.0-beta.1-source.tar.xz
cd akyuu-0.1.0-beta.1
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

Launch Akyuu from the application menu or run `~/.local/bin/akyuu`. Other distribution toolchains
have not been validated yet. There is no AppImage, Flatpak, `.deb` or `.rpm` package in this beta.
GitHub's automatic **Source code (zip/tar.gz)** files omit submodule contents; use our full-source
asset for an offline source build.

Read the [privacy notice](PRIVACY.md) before connecting an account. Release assets include
`SHA256SUMS`; after downloading all listed files to one directory, verify them with
`sha256sum -c SHA256SUMS`.

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

Read the [privacy notice](PRIVACY.md) before connecting an account.
[Release and versioning policy](RELEASING.md) describes how beta and stable releases are numbered.

## Building

Requirements: CMake 3.21+, Ninja, a C++23 compiler, and Qt 6 with the Concurrent, DBus, Network, Sql,
Svg, Widgets and LinguistTools modules. Qt Multimedia is optional. On Linux, media players are
read via sd-bus, so libsystemd is needed as well (`-DANISTHESIA_MPRIS=OFF` leaves it out, along
with web browser detection).

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

### Dependency sources

The exact commits are recorded as Git submodules; normal recursive clones use those commits,
not the newest branch tips. Anisthesia uses [our fork](https://github.com/ligneo/anisthesia) at
`6988ab4` for the native Linux work. Anitomy uses [Eren's upstream](https://github.com/erengy/anitomy)
at `8498b53`; it does not need a separate Akyuu fork push. The other upstream dependency pins
are included in the full-source release asset too.

### Related projects

- [Anime relations](https://github.com/erengy/anime-relations) (episode redirections)
- [Anisthesia](https://github.com/ligneo/anisthesia) (media detection library, with Linux support)
- [Anitomy](https://github.com/erengy/anitomy) (anime video filename parser)

## License

Akyuu is licensed under the [GNU General Public License v3](LICENSE).

Copyright (C) 2010-2026, Eren Okka (Taiga)<br>
Copyright (C) 2026, cenky
