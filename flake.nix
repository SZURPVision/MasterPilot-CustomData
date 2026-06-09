{
  description = "MasterPilot CustomData Protocol";

  inputs = {
    flake-parts.url = "github:hercules-ci/flake-parts";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" "x86_64-darwin" ];
      imports = [
        ./nix/check.nix
        ./nix/shells.nix
        ./nix/packages
        ./nix/apps
      ];
      perSystem = { system, ... }:
      {
        _module.args.pkgs = import inputs.nixpkgs {
          inherit system;
          overlays = [ (import ./nix/overlays.nix) ];
        };
      };
    };
}
