{ ... }:
{
  perSystem = { pkgs, config, ... }:
  let
    mkDist = dry-run: (import ./generate-dists.nix {
      inherit dry-run;
      inherit (pkgs) runCommand replaceVars makeWrapper git;
      cSrcPath = config.packages.customdata-c-src;
      cppSrcPath = config.packages.customdata-cpp-src;
    });
  in
  {
    apps = {
      generate-dists = {
        type = "app";
        program = "${mkDist true}/bin/generate-dists";
        meta = {
          description = "Generate all the dist branches, dry run.";
        };
      };
      generate-dists-no-dry-run = {
        type = "app";
        program = "${mkDist false}/bin/generate-dists";
        meta = {
          description = "Generate all the dist branches, no dry run.";
        };
      };
    };
  };
}