{ self, ... }:
{
  perSystem = { pkgs, config, ... }:
  {
    packages = {
      customdata-src = pkgs.callPackage ./customdata-src/package.nix {
        src = self;
        inherit (config.packages) nanopb-fixed;
      };
      # TODO: 未完成
      # customdata-cpp = pkgs.callPackage ./customdata-cpp/package.nix {
      #   src = self;
      #   inherit (config.packages) customdata-cpp-src;
      # };
      customdata-c-src = pkgs.callPackage ./customdata-c-src/package.nix {
        src = self;
        inherit (config.packages) customdata-src;
      };
      customdata-cpp-src = pkgs.callPackage ./customdata-cpp-src/package.nix {
        src = self;
        inherit (config.packages) customdata-src;
      };
      mp-customdata-test = pkgs.callPackage ./test/package.nix {
        src = self;
        inherit (config.packages) customdata-src;
      };
      default = config.packages.customdata-src;

      nanopb-fixed = pkgs.callPackage ./nanopb-fixed/package.nix { };
    };
  };
}
