{ src, stdenv, customdata-src, nanopb }:

stdenv.mkDerivation {
  name = "customdata-c-src";
  version = "0.1.0";
  meta = {
    descprtion = "MasterPilot CustomData - C source package";
  };
  dontUnpack = true;

  installPhase = ''
    mkdir -p $out/src $out/include

    cp ${src}/src/*.h $out/include/
    cp ${src}/src/nanopb/*.h $out/include/
    cp ${customdata-src}/c/*.h $out/include/
    cp ${nanopb.src}/*.h $out/include/
    
    cp ${src}/src/*.c $out/src/
    cp ${src}/src/nanopb/*.c $out/src/
    cp ${customdata-src}/c/*.c $out/src/
    cp ${nanopb.src}/*.c $out/src/
  '';

}
