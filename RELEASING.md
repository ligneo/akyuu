# Releases and versioning

Akyuu uses `MAJOR.MINOR.PATCH` with numbered prereleases, following
[Semantic Versioning](https://semver.org/). During initial development, our application convention
is:

| Change | Next version example |
| --- | --- |
| Another test build for the same release | `0.1.0-beta.1` -> `0.1.0-beta.2` |
| The tested release is ready | `0.1.0-beta.3` -> `0.1.0` |
| Compatible fixes, including security fixes | `0.1.0` -> `0.1.1` |
| A new feature set or a compatibility change during 0.x | `0.1.1` -> `0.2.0` |
| Documented, stable application contracts | `0.x.y` -> `1.0.0` |

Our compatibility contracts cover the command line and persisted settings, account and library
data. Document migrations and preserve user data when these change. After 1.0, incompatible
contract changes increment MAJOR; compatible features increment MINOR; fixes increment PATCH.
Reset smaller components when incrementing a larger one. Version numbers are integers, so
`beta.10` follows `beta.9`.

A commit is not a release. Collect useful changes, build and check them, then issue a release.
Development builds may retain the last release number; record the source commit to distinguish
them. The maintainer or assisting developer prepares the number, changelog, tag and artifacts
when a release is requested. Publishing and repository visibility follow the maintainer's
instructions. Do not schedule or publish releases merely because a commit landed.

## Source of truth

`src/akyuu/config.h` supplies the version to the application and CMake. Release commands
use the standalone C++ tool in `tools/release`; the optional Nu entry builds and runs it. Use:

```sh
nu tools/release.nu bump-version 0.1.0-beta.1
```

The tool validates the version, commits a change when necessary and creates an annotated
`v0.1.0-beta.1` tag. It does not push. A released tag and its artifacts are immutable; corrections
get a new version. Never move a released tag or replace a published binary under the same number.

## Release procedure

1. Check the working tree and finish the relevant changes. Update `CHANGELOG.md`, including
   known limitations and any data migration. Preserve upstream copyright and license notices.
2. Make sure every pinned submodule commit is available remotely. Do not update submodules with
   `--remote`: the release uses the exact commits recorded by the parent repository.
3. Commit the changes and run the version tool. Build the tagged tree, run the focused tests
   and test startup with isolated data. Reinstall the checked development build on the maintainer's
   tester machine when requested; a running process must be restarted to use a replaced binary.
4. Push the intended branch and **only the named release tag**:

   ```sh
   git push origin HEAD v0.1.0-beta.1
   ```

   This repository retains Taiga's history and inherited local tags. Never use `git push --tags`.
5. Verify a fresh checkout of that remote tag with `--recurse-submodules`. Build the distributable
   from the pinned sources. A source bundle must include every submodule and its license: GitHub's
   automatically generated source archives do not include submodule contents. Supply SHA-256
   checksums for uploaded artifacts. Describe binary platform and runtime dependencies explicitly.
6. Create the GitHub release using the tagged version and changelog. Mark `-beta.N` releases as
   prereleases. Verify downloaded assets against the checksums before publishing. Do not claim
   Windows or clean-chroot validation unless it was actually performed.
7. Update package recipes to this exact tag. Arch package versions omit the hyphen:
   `0.1.0-beta.1` becomes `0.1.0beta.1`; increment `pkgrel` only for packaging changes to unchanged
   upstream source. Publish to AUR only with an authorized maintainer account.

To run the focused checks in a build with the pinned dependencies:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAKYUU_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The update service follows stable releases for stable installations and also offers prereleases
for prerelease installations. It reads paginated GitHub release metadata and downloads only a
package matching the operating system, architecture and installed package format. An ambiguous
or unverified package falls back to the release page. SHA-256 and size are checked before an
atomic save; cancellation and failure leave any existing file intact. Installation remains with
the user or package manager.

Binary assets use these names, with `VERSION` matching the release tag without `v`:

- Linux AppImage: `akyuu-VERSION-x86_64.AppImage` or `akyuu-VERSION-arm64.AppImage`
- Arch: `akyuu-ARCHVERSION-PKGREL-x86_64.pkg.tar.zst` (prerelease hyphens removed)
- Debian/Ubuntu: `akyuu-VERSION-x86_64.deb`
- Fedora/openSUSE: `akyuu-VERSION-x86_64.rpm`
- Windows installer: `akyuu-VERSION-x86_64-setup.exe`
- macOS disk image: `akyuu-VERSION-arm64.dmg`

These are packaging contracts, not a claim that each platform has a published build. GitHub must
report the asset as uploaded, with its exact download URL, byte size and SHA-256 digest. Source
archives are never offered as installers. Build Arch packages with `-DAKYUU_PACKAGE_FORMAT=arch`
and AppImages with `-DAKYUU_PACKAGE_FORMAT=appimage`. Source builds default to the portable Linux
package on Linux; AppImage installations keep that format. Add another installer format in the
package model and its tests, without adding platform conditions to the main window.

## Linux AppImage

Build the tagged sources on an older supported Linux base, with `AKYUU_PORTABLE=OFF`,
`AKYUU_PACKAGE_FORMAT=appimage` and `CMAKE_INSTALL_PREFIX=/usr`. The beta.3 build uses
Ubuntu 24.04, GCC 16.2.1 and the official Qt 6.11.2 SDK. GCC 14 and 15 lack the required
`std::ranges::starts_with`; CMake checks the feature before building. Keep the compiler
runtime compatible with the build base, rather than copying the host's glibc.

Use verified linuxdeploy, its Qt plugin and a pinned AppImage runtime. Set `LINUXDEPLOY`,
`LINUXDEPLOY_QT`, `QMAKE`, `GCC_RUNTIME_DIR` and `LDAI_RUNTIME_FILE`, then run:

```sh
nu tools/release.nu package-appimage build AppDir licenses output
```

The tool requires a new AppDir, includes Qt's SQLite driver, X11 and Wayland plugins, and
keeps screen drivers on the host. `licenses` contains third-party copyright notices, full
license texts and a source inventory. Include the corresponding Qt, compiler runtime,
FFmpeg, ICU, system library and AppImage runtime sources alongside the release, with their
checksums. Users may extract an AppImage and replace its dynamic libraries; no installation
key or locked runtime is required. Akyuu's full-source archive is separate from this
third-party source archive.

Check the final file in separate Ubuntu and Fedora environments without a Qt SDK. Verify
startup, SQLite, HTTPS and required GLIBC symbols; document the measured minimum and the
limits of desktop testing. Recheck downloaded release assets, not just local build outputs.

## Automated packages

The Packages workflow builds and tests packages on version tags and manual runs.
Ordinary branch pushes and pull requests do not start the package jobs. A named `vVERSION`
tag also prepares a **draft** release after all build and installation checks pass. It does not publish automatically. Downloaded draft
assets must pass their checksums before the maintainer publishes it. Existing releases are
never overwritten.

Linux builds use the prepared x86_64 SDK in `setup/linux/Dockerfile`. Its entry point
accepts `build-linux output` to create the AppImage, DEB, RPM,
full committed source archive and corresponding third-party source archive. Only the update
package policy is rebuilt between formats; the GUI and download service are shared. Native
packages keep their runtime under `/opt/akyuu` and use the distribution's graphics drivers
and system C library. Their package versions sort numbered betas before the final release.

The workflow installs, upgrades from a synthetic older package fixture, starts and removes
packages in Ubuntu 24.04, Debian 13, Fedora 44 and openSUSE Leap 16.0. SQLite, TLS and
PNG/JPEG/SVG checks run without a Qt SDK on the installation path. These checks cover
packaging and runtime loading; real graphics, audio, Wayland and player integrations require
desktop testing. The compiler/tool hashes and exact bundled Ubuntu source package versions
are recorded in the corresponding source payload. The base image is pinned, while Ubuntu
security updates and the external aqt SDK downloader's Python dependencies can change between builds; this is not a
claim of byte-for-byte reproducibility.

Windows builds use MSVC 2022, Qt 6.11.2 and `windeployqt`, followed by the NSIS installer.
The C++ `package-windows` command verifies the official Microsoft runtime signature and creates a
removal manifest from the deployed files. The installation test runs only on a disposable
Windows Actions runner and verifies startup, TLS/database/image plugins, reinstall, and
preservation of user data and unrelated files during removal. User data lives in AppData;
the installer is not a portable build. The installer is unsigned. macOS is deferred.

An Arch package is built from the **same** full-source archive as the release; its recipe
and checksum are included. Publishing downloadable packages on GitHub requires no AUR,
Debian, Fedora or openSUSE account. Distribution repository submission and maintenance are
separate work. AUR publication remains deferred.

## Nix

The flake builds the recorded source and submodules with the locked Nixpkgs input.
It exposes `packages.x86_64-linux.akyuu` (also the default) and an overlay. Use Nix 2.27
or newer with `nix-command` and `flakes` enabled; the Git fetcher reads the submodules.
The version comes from `src/akyuu/config.h`, and the package uses `AKYUU_PACKAGE_FORMAT=nix`.
Nix installations use Nix for upgrades; the update dialog offers the guide instead of
an AppImage download. Installation examples live in the existing wiki guide.

`nix flake check` builds the package, runs CTest and checks the wrapped installed
application's startup with isolated data. The package workflow includes this check for
version tags and manual runs. A manual run can select only the Nix job; no package jobs
run on ordinary branch pushes or pull requests. Development builds may retain the last
release version; pin the Git revision when a fixed development build is required.
