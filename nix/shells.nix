{ inputs, ... }:
{
  perSystem = { pkgs, config, ... }:
  {
    devShells = {
      default = pkgs.mkShell {
        inputsFrom = [
          config.packages.customdata-generated
        ];
        packages = [
          pkgs.clang-tools
          pkgs.cmake
          pkgs.pkg-config
          pkgs.nanopb
          pkgs.protobuf
        ];
        shellHook = ''
          buf generate
          cmake -B build
          ln -s build/compile_commands.json ./
        '';
      };
    };
  };
}