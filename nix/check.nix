{ self, ... }:
{
  perSystem = { pkgs, ... }:
  {
    checks.buf-lint = pkgs.stdenv.mkDerivation {
      name = "buf-lint";
      src = self;
      nativeBuildInputs = [ pkgs.buf ];

      buildPhase = ''
        export HOME=$TMPDIR
        buf lint

        touch $out
      '';
    };
  };
}
