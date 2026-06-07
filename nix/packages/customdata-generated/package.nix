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
  fetchurl
}:
stdenv.mkDerivation {
  name = "customdata-generated";
  version = "0.1.0";

  meta = {
    description = "MasterPilot CustomData - buf generated source pack";
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
}