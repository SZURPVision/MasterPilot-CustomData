{ dry-run, runCommand, replaceVars, makeWrapper, git, embeddedSrcPath }:
let
  script = replaceVars ./generate-dists.sh.in {
    inherit dry-run embeddedSrcPath;
  };
in
runCommand "generate-dists" {
  nativeBuildInputs = [ makeWrapper ];
} ''
  mkdir -p $out/bin
  cp ${script} $out/bin/generate-dists
  chmod +x $out/bin/generate-dists
  wrapProgram $out/bin/generate-dists --prefix PATH : ${git}/bin
''