{
  src,
  stdenv,
  cmake,
  customdata-src,
  lib,
}:

stdenv.mkDerivation {
  pname = "customdata-cpp-src";
  version = "0.1.0";
  meta = {
    description = "MasterPilot CustomData - C++ source pacakge";
  };

  dontBuild = true;
  dontUnpack = true;

  installPhase = ''
    mkdir -p $out/include/masterpilot $out/include $out/src

    cp ${src}/src/*.h $out/include/masterpilot/
    cp ${src}/src/*.c $out/src/

    cp ${src}/src/cpp/*.cpp $out/src/
    cp ${src}/src/cpp/*.hpp $out/include/masterpilot/

    cp ${customdata-src}/cpp/*.pb.h $out/include/masterpilot/
    cp ${customdata-src}/cpp/*.pb.cc $out/src/
  '';
}