{
  src,
  stdenv,
  cmake,
  protobuf,
  abseil-cpp,
  customdata-generated,
  lib,
}:

stdenv.mkDerivation {
  inherit src;
  pname = "customdata-cpp";
  version = "0.1.0";

  meta = with lib; {
    description = "MasterPilot CustomData - C++ protobuf wrapper library";
    platforms = platforms.unix;
  };

  nativeBuildInputs = [ cmake ];
  buildInputs = [ protobuf abseil-cpp ];

  cmakeFlags = [
    "-DMINIMAL_MODE=ON"
    "-DBUILD_CPP=ON"
    "-DBUILD_TESTS=ON"
    "-DCUSTOMDATA_GENERATED_DIR=${customdata-generated}"
  ];

  doCheck = true;
  checkPhase = ''
    ./test/cpp/test-cpp-roundtrip
  '';
}
