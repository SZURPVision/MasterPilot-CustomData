{
  src,
  stdenv,
  protobuf,
  buf,
  python3,
  nanopb-fixed,
  lib,
  symlinkJoin,
  makeWrapper,
}:
stdenv.mkDerivation {
  name = "customdata-src";
  version = "0.1.0";

  meta = {
    descprtion = "MasterPilot CustomData - buf generated source pack";
  };

  inherit src;
  
  nativeBuildInputs = [
    protobuf
    buf
    nanopb-fixed
  ];

  configurePhase = ''
    export HOME=$TMP_DIR
  '';

  buildPhase = ''
    buf generate
  '';

  installPhase = ''
    cp -r generated $out
  '';

  passthru = {
    inherit nanopb-fixed;
  };
}