{
  description = "Tierra reimplementation";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    raygui-src = {
      url = "github:raysan5/raygui/4.0";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, raygui-src }:
    let
      forAllSystems = nixpkgs.lib.genAttrs [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
    in {
      devShells = forAllSystems (system:
        let pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.mkShell {
            packages = with pkgs; [ clang gnumake pkg-config raylib gdb ];
            env.RAYGUI_INCLUDE = "${raygui-src}/src";
          };
        });
    };
}
