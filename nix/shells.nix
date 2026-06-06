{ inputs, ... }:
{
  perSystem = { pkgs, config, ... }:
  {
    devShells = {
      # ── 默认开发环境：proto 生成 + C/C++ 开发工具链 ──
      default = pkgs.mkShell {
        inputsFrom = [
          config.packages.customdata-src
        ];
        packages = [
          pkgs.clang-tools
          pkgs.cmake
          pkgs.pkg-config
          pkgs.nanopb
          pkgs.protobuf
        ];
        # CMake find_package(Nanopb) 自动通过 Nix CMAKE_PREFIX_PATH 发现依赖
        # 不再需要手动设 CPATH
      };

      # ── 测试开发环境 ──
      test = pkgs.mkShell {
        inputsFrom = [
          config.packages.mp-customdata-test
        ];
        packages = [
          pkgs.clang-tools
          pkgs.cmake
          pkgs.gdb
        ];
        shellHook = ''
          cp -Rvf ${config.packages.mp-customdata-test.passthru.compile-commands}/compile_commands.json compile_commands.json
          chmod +w compile_commands.json
        '';
      };
    };
  };
}