# ClangSharp 太权威了兄弟
{
  src,
  clangsharp-generator,
  stdenv
}:
stdenv.mkDerivation {
  inherit src;
  pname = "customdata-core-binding-csharp";
  version = "0.1.0";

  nativeBuildInputs = [ clangsharp-generator ];

  buildPhase = ''
    ${clangsharp-generator}/bin/clangsharp-generator @core.rsp
  '';

  installPhase = ''
    mkdir $out
    cp -r generated $out/
  '';
}