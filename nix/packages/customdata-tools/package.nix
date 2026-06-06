{
  src,
  stdenv,
  cmake,
  nanopb,
  customdata-generated,
  lib,
}:

stdenv.mkDerivation {
  inherit src;
  pname = "mp-customdata-tools";
  version = "0.1.0";
  cmakeFlags = [
    "-DMINIMAL_MODE=ON"
    "-DBUILD_TOOLS=ON"
    "-DCMAKE_BUILD_TYPE=Debug"
    "-DCUSTOMDATA_GENERATED_DIR=${customdata-generated}"
  ];
  nativeBuildInputs = [ cmake ];
  buildInputs = [ nanopb ];

  postInstall = ''
    ln -s $out/bin/*customdata-tools $out/bin/customdata-tools
  '';
}