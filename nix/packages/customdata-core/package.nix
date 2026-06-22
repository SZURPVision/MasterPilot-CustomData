{
  src,
  stdenv,
  cmake,
  lib,
  customdata-generated,
  bash,
}:

stdenv.mkDerivation {
  inherit src;
  pname = "customdata-core";
  version = "0.1.0";

  meta = with lib; {
    description = "MasterPilot CustomData - Core block transport shared library";
    platforms = platforms.unix;
  };

  nativeBuildInputs = [ cmake ];

  cmakeFlags = [
    "-DMINIMAL_MODE=ON"
    "-DBUILD_CORE_SHARED=ON"
    "-DBUILD_TOOLS=ON"
    "-DCUSTOMDATA_GENERATED_DIR=${customdata-generated}"
  ];

  doCheck = true;
  checkPhase = ''
    bash "$src/test/tools-test.sh" "$(realpath src/tools/mp-customdata-tools)"
  '';
}
