# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Command orchestration only; packaging policy lives in tools/release/*.cpp.
export def --wrapped checked [program: string, ...arguments: string] {
    ^$program ...$arguments
    if $env.LAST_EXIT_CODE != 0 {
        error make {msg: $"($program) failed with exit code ($env.LAST_EXIT_CODE)"}
    }
}

def main [command: string, ...arguments: string, --qt: string] {
    let root = ($env.FILE_PWD | path dirname)
    let build = ($root | path join build release-tools)
    let prefix = if $qt == null { [] } else { [$"-DCMAKE_PREFIX_PATH=($qt)"] }
    checked cmake -S ($root | path join tools release) -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release ...$prefix
    checked cmake --build $build --parallel 2
    let executable = if $nu.os-info.name == windows { 'akyuu-release.exe' } else { 'akyuu-release' }
    checked ($build | path join bin $executable) --root $root $command ...$arguments
}
