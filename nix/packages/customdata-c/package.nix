{ src, stdenv, customdata-src, nanopb }:

stdenv.mkDerivation {
  name = "customdata-c";
  version = "0.1.0";
  dontUnpack = true;

  installPhase = ''
    mkdir -p $out/src $out/include

    cp ${src}/*.h $out/include/
    cp ${customdata-src}/c/*.h $out/include/
    cp ${nanopb.src}/*.h $out/include/
    
    cp ${src}/*.c $out/src/
    cp ${customdata-src}/c/*.c $out/src/
    cp ${nanopb.src}/*.c $out/src/
  '';
}
