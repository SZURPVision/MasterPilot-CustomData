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

      shellHook = ''
        echo "=== MasterPilot CustomData Dev Shell ==="
        echo "  buf generate      - Regenerate proto code"
        echo "  buf lint          - Lint proto files"
        echo "  nix run .#gen-all - Regenerate all"
        echo "  clangd            - C++ language server"
      '';
    };
  };
}
