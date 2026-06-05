{ inputs, ... }:
{
  perSystem = { pkgs, config, ... }:
  {
    devShells.default = pkgs.mkShell {
      packages = (config.packages.default.nativeBuildInputs)
      ++ [
        pkgs.clang-tools
        pkgs.cmake
        pkgs.pkg-config
      ];
      # TODO: 简单粗暴塞个路径, 后续写Test再细调
      shellHook = ''
        export CPATH="${pkgs.nanopb}/include/nanopb:${pkgs.protobuf}/include''${CPATH:+:$CPATH}"
      '';
    };
  };
}
