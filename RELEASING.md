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

`src/akyuu/config.h` supplies the version to the application and CMake. Use:

```sh
tools/bump-version.sh 0.1.0-beta.1
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
- Windows installer: `akyuu-VERSION-x86_64-setup.exe`
- macOS disk image: `akyuu-VERSION-arm64.dmg`

These are packaging contracts, not a claim that each platform has a published build. GitHub must
report the asset as uploaded, with its exact download URL, byte size and SHA-256 digest. Source
archives are never offered as installers. Build Arch packages with `-DAKYUU_PACKAGE_FORMAT=arch`
and AppImages with `-DAKYUU_PACKAGE_FORMAT=appimage`. Source builds default to the portable Linux
package on Linux; AppImage installations keep that format. Add another installer format in the
package model and its tests, without adding platform conditions to the main window.
