{
  src,
  stdenv,
  lib,
  dotnet-sdk,
  buildDotnetModule,
  customdata-core-binding-csharp
}:

buildDotnetModule {
  inherit src;
  projectFile = "src/csharp/CustomData.csproj";
  testProjectFile = "test/CustomData.Tests/CustomData.Tests.csproj";
  nugetDeps = ./nuget-deps.json;
  pname = "customdata-core-csharp";
  version = "0.1.0";

  # ClangSharp 太权威了兄弟.
  # 转译的绑定是C# native, 不需要搞动态链接问题
  postPatch = ''
    cp -rL ${customdata-core-binding-csharp}/generated/csharp/bindings ./src/csharp/generated/
  '';

  doCheck = true;

  meta = with lib; {
    description = "MasterPilot CustomData - C# core transport layer";
  };
}
