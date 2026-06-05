{ self, ... }:
{
  perSystem = { pkgs, config, ... }:
  {
    packages = {
      customdata-src = pkgs.callPackage ./customdata-src/package.nix {
        src = self;
        inherit (config.packages) nanopb-fixed;
      };
      customdata-cpp = pkgs.callPackage ./customdata-cpp/package.nix {
        inherit (config.packages) customdata-src;
      };
      customdata-c = pkgs.callPackage ./customdata-c/package.nix {
        inherit (config.packages) customdata-src;
      };
      default = config.packages.customdata-src;

      nanopb-fixed = pkgs.callPackage ./nanopb-fixed/package.nix { };
    };
  };
}
