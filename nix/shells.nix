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
        ];
        shellHook = ''
          buf generate
          cmake -B build
          ln -s build/compile_commands.json ./
        '';
      };
      csharp = pkgs.mkShell
      {
          inputsFrom = [
            config.devShells.default
          ];
          packages = [
            pkgs.dotnet-sdk
            config.packages.clangsharp-generator
          ];
          shellHook = ''
            rm -rf src/csharp/generated/bindings
            clangsharp-generator @core.rsp

			rm -rf src/csharp/generated
            ln -sf $(realpath generated/csharp/bindings) src/csharp/generated
          '';
      };
    };
  };
}