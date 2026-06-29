{ src, stdenv, cmake, customdata-generated, nanopb }:

stdenv.mkDerivation {
  inherit src;
  name = "customdata-embedded-src";
  version = "0.1.0";
  meta = {
    description = "MasterPilot CustomData - Embedded source package";
  };

  nativeBuildInputs = [ cmake ];
  buildInputs = [ nanopb ];

  cmakeFlags = [
    "-DMINIMAL_MODE=ON"
    "-DBUILD_EMBEDDED=ON"
    "-DBUILD_TESTS=ON"
    "-DSRC_DIST=ON"
    "-DCUSTOMDATA_GENERATED_DIR=${customdata-generated}"
    "-DNANOPB_SRC_DIR=${nanopb.src}"
  ];

  doCheck = true;
  checkPhase = ''
    ./test/embedded/test-embedded-roundtrip
  '';

  doInstallCheck = true;
  installCheckPhase = ''
    cmake -B "$TMPDIR/build-test" -S "$out"
    cmake --build "$TMPDIR/build-test"
  '';
}