{
  src,
  stdenv,
  cmake,
  nanopb,
  protobuf,
  customdata-src,
  lib,
}:

let
  pname = "mp-customdata-test";
  version = "0.1.0";
  # 公共 cmakeFlags — 主构建与 compile-commands 共用
  cmakeFlags = [
    "-DBUILD_TESTING=ON"
    "-DBUILD_CPP=OFF"
    "-DCMAKE_BUILD_TYPE=Debug"
    "-DCUSTOMDATA_SRC_DIR=${customdata-src}"
  ];
in
stdenv.mkDerivation {
  inherit pname version src;
  nativeBuildInputs = [ cmake ];
  buildInputs = [ nanopb protobuf ];
  inherit cmakeFlags;

  doCheck = true;
  checkPhase = ''
    ctest --output-on-failure
  '';

  installPhase = ''
    mkdir -p $out/bin
    cp test/mp-customdata-test $out/bin/
  '';

  # ── 仅 cmake configure，不触发生成/building ──
  passthru = {
    compile-commands = stdenv.mkDerivation {
      name = "${pname}-compile-commands";
      inherit src;
      nativeBuildInputs = [ cmake ];
      buildInputs = [ nanopb protobuf ];
      cmakeFlags = cmakeFlags ++ [ "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" ];
      dontBuild = true;
      doCheck = false;
      installPhase = ''
        mkdir -p $out
        cp compile_commands.json $out/
      '';
    };
  };
}