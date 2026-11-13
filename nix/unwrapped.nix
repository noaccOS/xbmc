{
  # The flake passes one with the date of the commit
  version ? "22.0-unstable",
  stdenv,
  lib,
  fetchFromGitHub,
  fetchurl,
  autoconf,
  automake,
  libtool,
  makeWrapper,
  pkg-config,
  cmake,
  yasm,
  python3Packages,
  libxcrypt,
  libgcrypt,
  libgpg-error,
  libunistring,
  boost,
  avahi,
  lame,
  exiv2,
  fmt,
  gettext,
  pcre2,
  yajl,
  fribidi,
  which,
  openssl,
  gperf,
  tinyxml,
  tinyxml-2,
  taglib,
  libssh,
  gtest,
  ncurses,
  spdlog,
  libxml2,
  alsa-lib,
  libGLU,
  libGL,
  ffmpeg,
  fontconfig,
  freetype,
  ftgl,
  libjpeg,
  libpng,
  libtiff,
  libmpeg2,
  libsamplerate,
  libmad,
  libogg,
  libvorbis,
  flac,
  libxslt,
  lzo,
  libcdio,
  libmodplug,
  libass,
  libbluray,
  libudfread,
  sqlite,
  libmysqlclient,
  nasm,
  gnutls,
  libva,
  libdrm,
  curl,
  bzip2,
  zip,
  unzip,
  mesa-demos,
  libcec,
  libcec_platform,
  dcadec,
  libuuid,
  crossguid,
  libmicrohttpd,
  bluez,
  cacert,
  doxygen,
  giflib,
  glib,
  harfbuzz,
  lcms2,
  libidn2,
  libpthread-stubs,
  libtasn1,
  libplist,
  p11-kit,
  zlib,
  flatbuffers,
  fstrcmp,
  nlohmann_json,
  lirc,
  mesa-gl-headers,
  x11Support ? true,
  libx11,
  xorgproto,
  libxt,
  libxmu,
  libxext,
  libxinerama,
  libxrandr,
  libxtst,
  libxfixes,
  xdpyinfo,
  libxdmcp,
  dbusSupport ? true,
  dbus,
  nfsSupport ? true,
  libnfs,
  pulseSupport ? true,
  libpulseaudio,
  pipewireSupport ? true,
  pipewire,
  rtmpSupport ? true,
  rtmpdump,
  sambaSupport ? true,
  samba,
  udevSupport ? true,
  udev,
  opticalSupport ? true,
  usbSupport ? false,
  libusb-compat-0_1,
  vdpauSupport ? true,
  libvdpau,
  waylandSupport ? false,
  wayland,
  wayland-protocols,
  waylandpp ? null,
  libxkbcommon,
  gbmSupport ? false,
  libgbm,
  libinput,
  libdisplay-info,
  # DRM PRIME decoding (eg. V4L2 Request API hwaccels) is only built with "gles"
  renderSystem ? if gbmSupport then "gles" else "gl",
  buildPackages,
}:

assert usbSupport -> !udevSupport; # libusb-compat-0_1 won't be used if udev is available
assert gbmSupport || waylandSupport || x11Support;
assert lib.elem renderSystem [
  "gl"
  "gles"
];

let
  # see https://github.com/xbmc/xbmc/blob/${kodiVersion}-${rel}/tools/depends/target/ to get suggested versions for all dependencies

  # FindLibDvd*.cmake always builds these internally on Linux (with meson),
  # so we only supply the tarballs. ExternalProject checks them against the
  # SHA512 in tools/depends/target/<lib>/<LIB>-VERSION, so they must match.
  libdvdcss = fetchurl {
    url = "http://mirrors.kodi.tv/build-deps/sources/libdvdcss-1.5.0.tar.bz2";
    sha512 = "439fbd9dae60b9a114d3429a19703478c734e8525ac6852da6f05f72d9ca44ca0ac5e874ab6a10017e7f31869fcd29f01d1bc156c4f0331b4e4abd98ec2f95cd";
  };

  libdvdnav = fetchurl {
    url = "http://mirrors.kodi.tv/build-deps/sources/libdvdnav-7.0.0.tar.bz2";
    sha512 = "8d12d476e352def9716ecbd7ba184a70dd9bae87dcde669b8647a0ef9ac1991f617c9918c043e159eb353302c4cc50fb71bc591ceaeba1d3b9d47af77fa72c71";
  };

  libdvdread = fetchurl {
    url = "http://mirrors.kodi.tv/build-deps/sources/libdvdread-7.0.1.tar.bz2";
    sha512 = "b5390a1acb8fdf6e24188f9259199eb0fef2dd4b332447ba2bfa571f9fc0c3bfad00c1d27a8b8806180a853c04a818b7802d3075ff787142c1a175ad798c6a3d";
  };

  # The python bindings refuse to build with SWIG < 4.5.0
  swig = buildPackages.swig.overrideAttrs (final: prev: {
    version = "4.5.0";
    src = fetchFromGitHub {
      owner = "swig";
      repo = "swig";
      tag = "v${final.version}";
      hash = "sha256-NXN3Zx66PNpYNuFVCalHymb/GW0cUi5drjIXNOVMgb8=";
    };
  });

  kodi_platforms =
    lib.optional gbmSupport "gbm"
    ++ lib.optional waylandSupport "wayland"
    ++ lib.optional x11Support "x11";
in
stdenv.mkDerivation (
  finalAttrs:
  let
    texturePacker = buildPackages.callPackage (
      {
        cmake,
        giflib,
        libjpeg,
        libpng,
        lzo,
        stdenv,
        zlib,
        pkg-config,
      }:
      stdenv.mkDerivation {
        pname = finalAttrs.pname + "-build-tool-texture-packer";
        inherit (finalAttrs) version src;

        sourceRoot = "${finalAttrs.src.name}/tools/depends/native/TexturePacker/src";

        nativeBuildInputs = [
          pkg-config
          cmake
        ];

        buildInputs = [
          giflib
          libjpeg
          libpng
          lzo
          zlib
        ];

        env.NIX_CFLAGS_COMPILE = lib.optionalString (!stdenv.hostPlatform.isWindows) "-DTARGET_POSIX";

        preConfigure = ''
          cmakeFlagsArray+=(-DKODI_SOURCE_DIR=$src -DCMAKE_MODULE_PATH=$src/cmake/modules)
        '';

        meta.mainProgram = "TexturePacker";
      }
    ) { };

    jsonSchemaBuilder = buildPackages.callPackage (
      { stdenv, cmake }:
      stdenv.mkDerivation {
        pname = finalAttrs.pname + "-build-tool-json-schema-builder";
        inherit (finalAttrs) version src;

        sourceRoot = "${finalAttrs.src.name}/tools/depends/native/JsonSchemaBuilder/src";

        nativeBuildInputs = [ cmake ];

        meta.mainProgram = "JsonSchemaBuilder";
      }
    ) { };
  in
  {
    pname = "kodi";
    inherit version;
    kodiReleaseName = "Piers";

    # This checkout, without the Nix files so changing them doesn't rebuild Kodi
    src = lib.fileset.toSource {
      root = ../.;
      fileset = lib.fileset.difference ../. (
        lib.fileset.unions [
          ../nix
          ../flake.nix
          ../flake.lock
        ]
      );
    };

    # make  derivations declared in the let binding available here, so
    # they can be overridden
    inherit
      libdvdcss
      libdvdnav
      libdvdread
      ;

    buildInputs = [
      gnutls
      libidn2
      libtasn1
      nasm
      p11-kit
      libxml2
      python3Packages.python
      boost
      libmicrohttpd
      exiv2
      fmt
      gettext
      pcre2
      yajl
      fribidi
      libva
      libdrm
      openssl
      gperf
      tinyxml
      tinyxml-2
      taglib
      libssh
      ncurses
      spdlog
      alsa-lib
      libGL
      libGLU
      fontconfig
      freetype
      ftgl
      libjpeg
      libpng
      libtiff
      libmpeg2
      libsamplerate
      libmad
      libogg
      libvorbis
      flac
      libxslt
      lzo
      libcdio
      libmodplug
      libass
      libbluray
      libudfread
      sqlite
      libmysqlclient
      avahi
      lame
      curl
      bzip2
      zip
      unzip
      mesa-demos
      libcec
      libcec_platform
      dcadec
      libuuid
      libxcrypt
      libgcrypt
      libgpg-error
      libunistring
      crossguid
      libplist
      bluez
      glib
      harfbuzz
      lcms2
      libpthread-stubs
      ffmpeg
      flatbuffers
      fstrcmp
      nlohmann_json
      lirc
      mesa-gl-headers

      # Deps needed by TexturePacker, which is built and installed in normal
      # kodi build, however the one used during the build is not this one
      # in order to support cross-compilation.
      giflib
      zlib
    ]
    ++ lib.optionals x11Support [
      libx11
      xorgproto
      libxt
      libxmu
      libxext.dev
      libxdmcp
      libxinerama
      libxrandr.dev
      libxtst
      libxfixes
    ]
    ++ lib.optional dbusSupport dbus
    ++ lib.optional nfsSupport libnfs
    ++ lib.optional pulseSupport libpulseaudio
    ++ lib.optional pipewireSupport pipewire
    ++ lib.optional rtmpSupport rtmpdump
    ++ lib.optional sambaSupport samba
    ++ lib.optional udevSupport udev
    ++ lib.optional usbSupport libusb-compat-0_1
    ++ lib.optional vdpauSupport libvdpau
    ++ lib.optionals waylandSupport [
      wayland
      waylandpp.dev
      wayland-protocols
      # Not sure why ".dev" is needed here, but CMake doesn't find libxkbcommon otherwise
      libxkbcommon.dev
    ]
    ++ lib.optionals gbmSupport [
      libxkbcommon.dev
      libgbm
      libinput.dev
      libdisplay-info
    ];

    nativeBuildInputs = [
      cmake
      doxygen
      makeWrapper
      which
      pkg-config
      autoconf
      automake
      libtool # still needed for some components. Check if that is the case with 19.0
      yasm
      gettext
      python3Packages.python
      flatbuffers
    ]
    ++ lib.optionals waylandSupport [
      wayland-protocols
      waylandpp.bin
    ];

    nativeCheckInputs = [
      gtest
    ];

    depsBuildBuild = [
      buildPackages.stdenv.cc
    ];

    cmakeFlags = [
      "-DAPP_RENDER_SYSTEM=${renderSystem}"
      "-DLIBDVDCSS_URL=${finalAttrs.libdvdcss}"
      "-DLIBDVDNAV_URL=${finalAttrs.libdvdnav}"
      "-DLIBDVDREAD_URL=${finalAttrs.libdvdread}"
      # Passed as paths rather than nativeBuildInputs, so their setup hooks
      # don't take over the configure/build phases from cmake
      "-DMESON_EXECUTABLE=${lib.getExe buildPackages.meson}"
      "-DNINJA_EXECUTABLE=${lib.getExe buildPackages.ninja}"
      # Upstream derives this from the git HEADs hash and date.
      # LibreElec (minimal distro for kodi) uses the equivalent to this.
      "-DGIT_VERSION=${finalAttrs.version}-${finalAttrs.kodiReleaseName}"
      "-DENABLE_EVENTCLIENTS=ON"
      "-DENABLE_INTERNAL_CROSSGUID=OFF"
      "-DENABLE_INTERNAL_NLOHMANNJSON=OFF"
      "-DENABLE_OPTICAL=${if opticalSupport then "ON" else "OFF"}"
      "-DENABLE_VDPAU=${if vdpauSupport then "ON" else "OFF"}"
      "-DSWIG_EXECUTABLE=${lib.getExe swig}"
      "-DFLATBUFFERS_FLATC_EXECUTABLE=${buildPackages.flatbuffers}/bin/flatc"
      "-DPYTHON_EXECUTABLE=${buildPackages.python3Packages.python}/bin/python"
      "-DPYTHON_LIB_PATH=${python3Packages.python.sitePackages}"
      "-DWITH_JSONSCHEMABUILDER=${lib.getExe jsonSchemaBuilder}"
      # When wrapped KODI_HOME will likely contain symlinks to static assets
      # that Kodi's built in webserver will cautiously refuse to serve up
      # (because their realpaths are outside of KODI_HOME and the other
      # whitelisted directories). This adds the entire nix store to the Kodi
      # webserver whitelist to avoid this problem.
      "-DKODI_WEBSERVER_EXTRA_WHITELIST=${builtins.storeDir}"
    ]
    ++ lib.optionals waylandSupport [
      "-DWAYLANDPP_SCANNER=${buildPackages.waylandpp}/bin/wayland-scanner++"
    ]
    ++ lib.optionals (stdenv.hostPlatform != stdenv.buildPlatform) [
      "-DWITH_TEXTUREPACKER=${lib.getExe texturePacker}"
    ];

    doCheck = stdenv.buildPlatform.canExecute stdenv.hostPlatform;
    checkPhase = ''
      runHook preCheck

      make -j $NIX_BUILD_CORES kodi-test

      ./kodi-test --gtest_filter=-${
        lib.concatStringsSep ":" [
          "TestCPUInfo.GetCPUFrequency"
          "TestNetwork.PingHost"
          "TestSystemInfo.GetOsName"
          "TestSystemInfo.GetOsPrettyNameWithVersion"
        ]
      }

      runHook postCheck
    '';

    preConfigure = ''
      cmakeFlagsArray+=("-DCORE_PLATFORM_NAME=${lib.concatStringsSep " " kodi_platforms}")
    '';

    postInstall = ''
      # TODO: figure out which binaries should be wrapped this way and which shouldn't
      for p in $(ls --ignore=kodi-send $out/bin/) ; do
        wrapProgram $out/bin/$p \
          --prefix PATH ":" "${
            lib.makeBinPath (
              [
                python3Packages.python
                mesa-demos
              ]
              ++ lib.optional x11Support xdpyinfo
              ++ lib.optional sambaSupport samba
            )
          }" \
          --prefix LD_LIBRARY_PATH ":" "${
            lib.makeLibraryPath (
              [
                curl
                libmad
                libcec
                libcec_platform
                libass
              ]
              ++ lib.optional vdpauSupport libvdpau
              ++ lib.optional nfsSupport libnfs
              ++ lib.optional rtmpSupport rtmpdump
            )
          }"
      done

      wrapProgram $out/bin/kodi-send \
        --prefix PYTHONPATH : $out/${python3Packages.python.sitePackages}

      substituteInPlace $out/share/xsessions/kodi.desktop \
        --replace kodi-standalone $out/bin/kodi-standalone

      rm $out/share/kodi/system/certs/cacert.pem
      ln -s ${cacert}/etc/ssl/certs/ca-bundle.crt $out/share/kodi/system/certs/cacert.pem
    '';

    doInstallCheck = true;

    installCheckPhase = "$out/bin/kodi --version";

    passthru = {
      pythonPackages = python3Packages;
      ffmpeg = ffmpeg;
      kodi = finalAttrs.finalPackage;
    };

    meta = {
      description = "Media center";
      homepage = "https://kodi.tv/";
      license = lib.licenses.gpl2Plus;
      platforms = lib.platforms.linux;
      teams = [ lib.teams.kodi ];
      mainProgram = "kodi";
    };
  }
)
