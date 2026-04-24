{
  description = "Nix development environment for MasterPilot custom protobuf definitions";

  inputs = {
    nixpkgs.url = "nixpkgs";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      forAllSystems = f:
        nixpkgs.lib.genAttrs supportedSystems (system:
          f (import nixpkgs { inherit system; }));

      mkTooling = pkgs:
        let
          pythonEnv = pkgs.python3.withPackages (ps:
            with ps; [
              protobuf
              grpcio-tools
            ]);

          mkRepoCommand = { name, runtimeInputs, text }:
            pkgs.writeShellApplication {
              inherit name runtimeInputs;
              text = ''
                if [ ! -d src ] || [ ! -d nanopb ]; then
                  echo "Run this command from the repository root." >&2
                  exit 1
                fi

                ${text}
              '';
            };

          genNanopb = mkRepoCommand {
            name = "generate-nanopb";
            runtimeInputs = [
              pkgs.protobuf
              pythonEnv
            ];
            text = ''
              out_dir="''${1:-generated/c}"
              mkdir -p "$out_dir"
              python3 nanopb/generator/nanopb_generator.py src/*.proto -I src -D "$out_dir"
            '';
          };

          genCpp = mkRepoCommand {
            name = "generate-cpp-proto";
            runtimeInputs = [ pkgs.protobuf ];
            text = ''
              out_dir="''${1:-generated/cpp}"
              mkdir -p "$out_dir"
              protoc --proto_path=src --cpp_out="$out_dir" src/*.proto
            '';
          };

          genCsharp = mkRepoCommand {
            name = "generate-csharp-proto";
            runtimeInputs = [ pkgs.protobuf ];
            text = ''
              out_dir="''${1:-generated/csharp}"
              mkdir -p "$out_dir"
              protoc --proto_path=src --csharp_out="$out_dir" src/*.proto
            '';
          };

          genAll = mkRepoCommand {
            name = "generate-all";
            runtimeInputs = [
              genNanopb
              genCpp
              genCsharp
            ];
            text = ''
              out_root="''${1:-generated}"
              generate-nanopb "$out_root/c"
              generate-cpp-proto "$out_root/cpp"
              generate-csharp-proto "$out_root/csharp"
            '';
          };
        in {
          inherit pythonEnv genNanopb genCpp genCsharp genAll;
        };
    in {
      devShells = forAllSystems (pkgs:
        let
          tooling = mkTooling pkgs;
        in {
          default = pkgs.mkShell {
            packages = [
              pkgs.protobuf
              pkgs.cmake
              pkgs.pkg-config
              pkgs.scons
              tooling.pythonEnv
              tooling.genNanopb
              tooling.genCpp
              tooling.genCsharp
              tooling.genAll
            ];

            shellHook = ''
              export PROTOC="${pkgs.protobuf}/bin/protoc"
              export NANOPB_GENERATOR="python3 nanopb/generator/nanopb_generator.py"

              echo "MasterPilot custom-data dev shell"
              echo "Available commands:"
              echo "  generate-nanopb [output-dir]"
              echo "  generate-cpp-proto [output-dir]"
              echo "  generate-csharp-proto [output-dir]"
              echo "  generate-all [output-root]"
            '';
          };
        });

      packages = forAllSystems (pkgs:
        let
          tooling = mkTooling pkgs;
        in {
          default = tooling.genNanopb;
          generate-nanopb = tooling.genNanopb;
          generate-cpp-proto = tooling.genCpp;
          generate-csharp-proto = tooling.genCsharp;
          generate-all = tooling.genAll;
        });

      apps = forAllSystems (pkgs:
        let
          tooling = mkTooling pkgs;
        in {
          default = {
            type = "app";
            program = "${tooling.genNanopb}/bin/generate-nanopb";
          };

          generate-nanopb = {
            type = "app";
            program = "${tooling.genNanopb}/bin/generate-nanopb";
          };

          generate-cpp-proto = {
            type = "app";
            program = "${tooling.genCpp}/bin/generate-cpp-proto";
          };

          generate-csharp-proto = {
            type = "app";
            program = "${tooling.genCsharp}/bin/generate-csharp-proto";
          };

          generate-all = {
            type = "app";
            program = "${tooling.genAll}/bin/generate-all";
          };
        });

      formatter = forAllSystems (pkgs: pkgs.nixfmt-rfc-style);
    };
}
