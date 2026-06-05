{ stdenv, cmake, protobuf, customdata-src }:

stdenv.mkDerivation {
  name = "customdata-cpp";
  version = "0.1.0";
  dontUnpack = true;
  nativeBuildInputs = [ cmake ];
  buildInputs = [ protobuf ];

  preConfigure = ''
    cp -r ${customdata-src}/cpp/* .
    cp ${./CMakeLists.txt} ./CMakeLists.txt
  '';

  cmakeFlags = [
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    "-DCMAKE_BUILD_TYPE=Release"
  ];

  postInstall = ''
    mkdir -p $out/share/customdata-cpp
    cp ${./CMakeLists.txt} $out/share/customdata-cpp/
  '';
}
