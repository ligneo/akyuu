# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
{
  description = "Akyuu anime list tracker";
  inputs.self.submodules = true;
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux";
      makePackage =
        pkgs:
        pkgs.callPackage ./nix/package.nix {
          stdenv = pkgs.gcc16Stdenv;
          src = self;
        };
      package = makePackage nixpkgs.legacyPackages.${system};
    in
    {
      packages.${system} = {
        akyuu = package;
        default = package;
      };
      checks.${system}.akyuu = package;
      overlays.default = final: prev: { akyuu = makePackage final; };
    };
}
