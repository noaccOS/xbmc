# Kodi 22 development shell, for rebuilding Kodi incrementally (with ccache) instead of a full nix
# build for every change. From the checkout:
#   nix develop
#   kodi-configure          # once, or after changing CMake files
#   cmake --build build
#   kodi-run <checkout>     # runs <checkout>/build/kodi.bin on Wayland with your usual ~/.kodi
# Add-ons installed through Nix (withPackages) aren't visible to this build.
{
  lib,
  mkShell,
  writeShellScriptBin,
  ccache,
  ninja,
  curl,
  libmad,
  libcec,
  libcec_platform,
  libass,
  libvdpau,
  libnfs,
  rtmpdump,
  kodi,
}:
let
  # Configures ./build like the package does. Kodi's CMake uses ccache when it's on PATH.
  kodi-configure = writeShellScriptBin "kodi-configure" ''
    exec cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      ${lib.escapeShellArgs kodi.cmakeFlags} \
      "-DCORE_PLATFORM_NAME=wayland x11" "$@"
  '';

  # Adds the libraries the package's wrapper provides for those Kodi loads at runtime
  kodi-run = writeShellScriptBin "kodi-run" ''
    if [ $# -lt 1 ]; then
      echo "usage: kodi-run <xbmc checkout> [kodi arguments...]" >&2
      exit 1
    fi
    checkout=$1
    shift

    export LD_LIBRARY_PATH=${
      lib.makeLibraryPath [
        curl
        libmad
        libcec
        libcec_platform
        libass
        libvdpau
        libnfs
        rtmpdump
      ]
    }''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
    # The kodi-<platform> launchers only exist once installed; kodi.bin runs from the build tree
    exec "$checkout/build/kodi.bin" --windowing=wayland "$@"
  '';
in
mkShell {
  inputsFrom = [ kodi ];
  packages = [
    ccache
    ninja
    kodi-configure
    kodi-run
  ];
}
