{
  src,
  stdenv,
  cmake,
  customdata-generated,
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
    mp_inc=$out/include/masterpilot
    mkdir -p $mp_inc $mp_inc/proto $out/include $out/src

    cp ${src}/src/core/*.h $mp_inc/
    cp ${src}/src/core/*.c $out/src/

    cp ${src}/src/cpp/*.hpp $mp_inc/
    cp ${src}/src/cpp/*.cpp $out/src/

    cp ${customdata-generated}/cpp/masterpilot/proto/*.pb.h $mp_inc/proto/
    cp ${customdata-generated}/cpp/masterpilot/proto/*.pb.cc $out/src/
  '';
}