# Kodi 22 "Piers" built from this checkout, ahead of nixpkgs (still on 21
# "Omega"). Mirrors nixpkgs' pkgs/applications/video/kodi/default.nix, but
# reuses its wrapper and add-on set.
# Based on nixpkgs pull request 450717.
{ callPackage, path, ... }@args:
let
  unwrapped = callPackage ./unwrapped.nix (
    removeAttrs args [
      "callPackage"
      "path"
    ]
  );
in
unwrapped.overrideAttrs (oldAttrs: {
  passthru =
    let
      finalKodi = oldAttrs.passthru.kodi;
    in
    oldAttrs.passthru
    // {
      packages = callPackage "${path}/pkgs/top-level/kodi-packages.nix" { kodi = finalKodi; };
      withPackages =
        func:
        callPackage "${path}/pkgs/applications/video/kodi/wrapper.nix" {
          kodi = finalKodi;
          addons = finalKodi.packages.requiredKodiAddons (func finalKodi.packages);
        };
    };
})
