{
  src,
  stdenv,
  lib,
  dotnet-sdk,
  buildDotnetModule,
  customdata-core-binding-csharp
}:

buildDotnetModule {
  src = "${src}/src/csharp";
  pname = "customdata-core-csharp";
  version = "0.1.0";

  postPatch = ''
    cp -rL ${customdata-core-binding-csharp}/generated/bindings ./generated/
  '';

  postInstall = ''
    mkdir -p $out/share/nuget
    dotnet pack \
      --no-build \
      --configuration Release \
      --output $out/share/nuget
  '';
  
  meta = with lib; {
    description = "MasterPilot CustomData - C# core transport layer";
  };
}