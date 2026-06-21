{ src, stdenv, customdata-generated, nanopb }:

stdenv.mkDerivation {
  name = "customdata-enbedded-src";
  version = "0.1.0";
  meta = {
    description = "MasterPilot CustomData - Embedded source package";
  };
  dontUnpack = true;

  installPhase = ''
    mp_inc=$out/include/masterpilot
    mkdir -p $out/src $out/include $mp_inc $mp_inc/proto

    find \${src}/src/core/ -maxdepth 1 -name "*.h" ! -name "*ffi*" -exec cp -t $mp_inc/ {} +
    cp ${src}/src/embedded/*.h $mp_inc/
    cp ${customdata-generated}/c/masterpilot/proto/*.h $mp_inc/proto/
    cp ${nanopb.src}/*.h $out/include/

    find \${src}/src/core/ -maxdepth 1 -name "*.c" ! -name "*ffi*" -exec cp -t $out/src/ {} +
    cp ${src}/src/embedded/*.c $out/src/
    cp ${customdata-generated}/c/masterpilot/proto/*.c $out/src/
    cp ${nanopb.src}/*.c $out/src/
  '';

}