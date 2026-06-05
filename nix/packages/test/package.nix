{ src, stdenv, cmake, customdata-c }:

stdenv.mkDerivation {
  inherit src;
  name = "mp-customdata-test";
  version = "0.1.0";
  nativeBuildInputs = [ cmake ];
  buildInputs = [ customdata-c ];

  cmakeFlags = [
    "-DCUSTOMDATA_C_DIR=${customdata-c}"
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    "-DCMAKE_BUILD_TYPE=Debug"
  ];

  doCheck = true;
  checkPhase = ''
    ctest --output-on-failure
  '';

  installPhase = ''
    mkdir -p $out/bin
    cp mp-customdata-test $out/bin/
  '';
}