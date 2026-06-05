{
  stdenv,
  nanopb,
  python3,
  makeWrapper
}:
let
  pythonWithDeps = python3.withPackages (ps: [
    ps.protobuf
    nanopb.python-module
  ]);
in
stdenv.mkDerivation {
  name = "nanopb-fixed-${nanopb.version}";
  
  dontUnpack = true;
  dontBuild = true;

  nativeBuildInputs = [ makeWrapper ];

  installPhase = ''
    mkdir -p $out/bin

    ln -s ${nanopb}/include $out/include;
    ln -s ${nanopb}/share $out/share;
    ln -s ${nanopb}/lib $out/lib;

    cp ${nanopb}/bin/protoc-gen-nanopb $out/bin/protoc-gen-nanopb
    chmod +w $out/bin/protoc-gen-nanopb

    wrapProgram $out/bin/protoc-gen-nanopb \
      --prefix PYTHONPATH : "${pythonWithDeps}/${python3.sitePackages}"
  '';
}