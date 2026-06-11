{
  src,
  stdenv,
  cmake,
  lib,
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
  ];
}