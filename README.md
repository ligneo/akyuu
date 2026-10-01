# Akyuu

[![](https://img.shields.io/github/license/ligneo/akyuu)](LICENSE)

Akyuu is an open-source desktop anime tracker for Linux. It detects the anime videos you watch on
your computer and synchronizes your progress with [AniList](https://anilist.co),
[Kitsu](https://kitsu.app) or [MyAnimeList](https://myanimelist.net). It helps you manage your
anime library, follow the season, share watched episodes and download new ones.

> **Status:** beta. Intended for daily use and feedback; known limitations are listed in the
> [changelog](CHANGELOG.md).

Download a published build from [Releases](https://github.com/ligneo/akyuu/releases).
See the [wiki](https://github.com/ligneo/akyuu/wiki) for installation and documentation.

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

Read the [privacy notice](PRIVACY.md) before connecting an account.

## Related projects

- [Anime relations](https://github.com/erengy/anime-relations) (episode redirections)
- [Anisthesia](https://github.com/ligneo/anisthesia) (media detection library, with Linux support)
- [Anitomy](https://github.com/erengy/anitomy) (anime video filename parser)

## License

Akyuu is licensed under the [GNU General Public License v3](LICENSE).

Copyright (C) 2010-2026, Eren Okka (Taiga)<br>
Copyright (C) 2026, cenky
