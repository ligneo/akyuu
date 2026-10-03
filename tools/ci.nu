# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
use release.nu checked

# Keep imports next to this file, independent of the runner's temporary scripts.
def main [step: string] {
    match $step {
        windows-sdk => {
            checked python -m pip install git+https://github.com/miurahr/aqtinstall.git@8c3695d4a4e1ceabf6a74dc6c79681656dc6b74b
            let extractor = ($env.ProgramFiles | path join 7-Zip 7z.exe)
            if not ($extractor | path exists) { error make {msg: 'The Windows build image requires 7-Zip.'} }
            $env.AQT_CONFIG = ($env.RUNNER_TEMP | path join akyuu-aqt.ini)
            "[aqt]\nconcurrency = 1\n" | save --force $env.AQT_CONFIG
            mkdir C:/Qt/6.11.2/msvc2022_64
            checked python -m aqt install-qt windows desktop 6.11.2 win64_msvc2022_64 --outputdir C:/Qt --external $extractor
        }
        windows-build => {
            $env.PATH = ($env.PATH | prepend 'C:/Qt/6.11.2/msvc2022_64/bin')
            checked cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/msvc2022_64 -DAKYUU_PORTABLE=OFF -DAKYUU_BUILD_TESTS=ON $"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=($env.PWD)/build/bin"
            checked cmake --build build --parallel 2
            checked ctest --test-dir build --output-on-failure
            checked nu tools/release.nu source-tests --qt C:/Qt/6.11.2/msvc2022_64
            checked ctest --test-dir build/release-tools --output-on-failure
        }
        windows-package => {
            $env.PATH = ($env.PATH | prepend 'C:/Qt/6.11.2/msvc2022_64/bin')
            let tool = ($env.PWD | path join build release-tools bin akyuu-release.exe)
            checked $tool prepare-windows-licenses packages/windows-licenses
            checked $tool package-windows build C:/Qt/6.11.2/msvc2022_64 packages/windows-licenses packages
            checked $tool test-windows (glob packages/*-setup.exe | first) ($env.PWD | path join build tests akyuu-deployment-tests.exe)
        }
        linux-build => {
            checked docker build -f setup/linux/Dockerfile -t akyuu-build .
            checked docker run --rm -v $"($env.PWD):/work" akyuu-build build-linux /work/packages
        }
        install-tools => {
            if not (which apt-get | is-empty) {
                checked apt-get update
                checked apt-get install -y --no-install-recommends ca-certificates git xvfb xauth
            } else if not (which dnf | is-empty) {
                checked dnf install -y git xorg-x11-server-Xvfb xauth
            } else {
                checked zypper --non-interactive install git xorg-x11-server-Xvfb xauth
            }
        }
        test-linux => {
            let tool = ($env.PWD | path join packages diagnostics akyuu-release.AppImage)
            checked chmod +x $tool
            checked $tool --appimage-extract-and-run test-linux $env.FORMAT packages
        }
        arch-build => {
            checked cmake -S tools/release -B build/release-tools -G Ninja -DCMAKE_BUILD_TYPE=Release
            checked cmake --build build/release-tools --parallel 2
            checked chown -R builder:builder .
            let archive = (glob packages/*-source.tar.xz | first | path expand)
            checked runuser -u builder -- ($env.PWD | path join build release-tools bin akyuu-release) --root $env.PWD package-arch $archive packages
        }
        release-draft => {
            let tool = ($env.PWD | path join packages diagnostics akyuu-release.AppImage)
            checked chmod +x $tool
            checked $tool --appimage-extract-and-run create-release-draft $env.GITHUB_REF_NAME packages
        }
        _ => { error make {msg: $"Unknown CI step: ($step)"} }
    }
}
