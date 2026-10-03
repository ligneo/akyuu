# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
{
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  qt6,
  systemd,
  xdg-utils,
  src,
}:

let
  config = builtins.readFile "${src}/src/akyuu/config.h";
  number = name: builtins.head (builtins.match ".*#define AKYUU_VERSION_${name} +([0-9]+).*" config);
  prerelease = builtins.head (builtins.match ''.*#define AKYUU_VERSION_PRE +"([^"]*)".*'' config);
in
stdenv.mkDerivation {
  pname = "akyuu";
  version =
    "${number "MAJOR"}.${number "MINOR"}.${number "PATCH"}"
    + lib.optionalString (prerelease != "") "-${prerelease}";
  src = lib.cleanSourceWith {
    inherit src;
    name = "akyuu-source";
    filter =
      path: type:
      let
        top = builtins.head (lib.splitString "/" (lib.removePrefix "${src}/" path));
      in
      builtins.elem top [
        "CMakeLists.txt"
        "cmake"
        "deps"
        "src"
        "tests"
        "LICENSE"
        "PRIVACY.md"
      ];
  };

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    qt6.qttools
    qt6.wrapQtAppsHook
  ];
  buildInputs = [
    qt6.qtbase
    qt6.qtsvg
    qt6.qtwayland
    qt6.qtmultimedia
    systemd
  ];
  cmakeFlags = [
    (lib.cmakeBool "AKYUU_BUILD_TESTS" true)
    (lib.cmakeBool "AKYUU_PORTABLE" false)
    (lib.cmakeFeature "AKYUU_PACKAGE_FORMAT" "nix")
  ];
  qtWrapperArgs = [
    "--prefix"
    "PATH"
    ":"
    (lib.makeBinPath [ xdg-utils ])
  ];

  doCheck = true;
  checkPhase = ''
    runHook preCheck
    export QT_QPA_PLATFORM=offscreen
    export XDG_RUNTIME_DIR="$TMPDIR/runtime"
    mkdir -m700 "$XDG_RUNTIME_DIR"
    wrapQtApp "$PWD/tests/akyuu-deployment-tests"
    ctest --output-on-failure
    runHook postCheck
  '';

  postInstall = ''
    install -Dm644 ../LICENSE "$out/share/licenses/akyuu/LICENSE"
  '';

  doInstallCheck = true;
  installCheckPhase = ''
    runHook preInstallCheck
    export QT_QPA_PLATFORM=offscreen
    export XDG_CONFIG_HOME="$TMPDIR/config"
    export XDG_DATA_HOME="$TMPDIR/data"
    export DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent
    mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME/akyuu/data"
    cat > "$XDG_DATA_HOME/akyuu/data/settings.json" <<'EOF'
    {"app.autoStart": true, "track.detection.enabled": false}
    EOF
    timeout 8s "$out/bin/akyuu" --minimized > "$TMPDIR/startup.log" 2>&1 && result=0 || result=$?
    if [ "$result" -ne 124 ]; then
      cat "$TMPDIR/startup.log"
      exit 1
    fi
    grep -Fx 'Exec=akyuu' "$XDG_CONFIG_HOME/autostart/Akyuu.desktop"
    runHook postInstallCheck
  '';

  meta = {
    description = "Anime list tracker with player and browser detection";
    homepage = "https://github.com/ligneo/akyuu";
    license = lib.licenses.gpl3Plus;
    mainProgram = "akyuu";
    platforms = [ "x86_64-linux" ];
  };
}
