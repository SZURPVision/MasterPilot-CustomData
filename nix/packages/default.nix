{ self, ... }:
{
  perSystem = { pkgs, config, ... }:
  {
    packages = {
      customdata-generated = pkgs.callPackage ./customdata-generated/package.nix {
        src = self;
        inherit (config.packages) nanopb-fixed;
      };
      # 下游导出用源码包（给没装 Nix 的环境）
      customdata-c-src = pkgs.callPackage ./customdata-c-src/package.nix {
        src = self;
        inherit (config.packages) customdata-generated;
      };
      customdata-cpp-src = pkgs.callPackage ./customdata-cpp-src/package.nix {
        src = self;
        inherit (config.packages) customdata-generated;
      };
      customdata-tools = pkgs.callPackage ./customdata-tools/package.nix {
        src = self;
        inherit (config.packages) customdata-generated;
      };
      default = config.packages.customdata-generated;

      nanopb-fixed = pkgs.callPackage ./nanopb-fixed/package.nix { };
    };
  };
}