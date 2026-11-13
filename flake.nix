{
  description = "Kodi 22 with the tempo sync fixes and mpv's scaletempo2";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
      version = "22.0-unstable-${
        builtins.substring 0 8 (self.lastModifiedDate or "19700101")
      }-${self.shortRev or "dirty"}";
    in
    {
      packages = forAllSystems (pkgs: rec {
        # Unwrapped like nixpkgs' kodi, add add-ons with kodi.withPackages
        kodi = pkgs.callPackage ./nix/package.nix { inherit version; };
        kodi-wayland = pkgs.callPackage ./nix/package.nix {
          inherit version;
          waylandSupport = true;
        };
        default = kodi-wayland;
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.callPackage ./nix/shell.nix {
          kodi = self.packages.${pkgs.stdenv.hostPlatform.system}.default;
        };
      });

      formatter = forAllSystems (pkgs: pkgs.nixfmt-tree);
    };
}
