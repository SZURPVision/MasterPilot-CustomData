{ inputs, ... }:
{
  perSystem = { pkgs, config, ... }:
  {
    devShells =
    let
    

    in {
      default = pkgs.mkShell 
      {
        inputsFrom = [
          config.packages.customdata-generated
        ];
        packages = [
          pkgs.clang-tools
          pkgs.cmake
          pkgs.pkg-config
          pkgs.protobuf

          pkgs.dotnet-sdk
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