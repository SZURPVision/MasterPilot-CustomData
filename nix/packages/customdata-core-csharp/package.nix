{
  src,
  buildDotnetModule,
  customdata-core-binding-csharp
}:

buildDotnetModule {
  inherit src;
  projectFile = "src/csharp/CustomData.csproj";
  testProjectFile = "test/CustomData.Tests/CustomData.Tests.csproj";
  nugetDeps = ./nuget-deps.json;
  pname = "customdata-core-csharp";
  version = "0.1.1";

  # ClangSharp 太权威了兄弟.
  # 转译的绑定是C# native, 不需要搞动态链接问题
  postPatch = ''
    cp -rL ${customdata-core-binding-csharp}/generated/csharp/bindings ./src/csharp/generated/
  '';
  packNupkg = true;
  doCheck = true;

  # buildDotnetModule 的 dotnetPack 对纯类库使用 --runtime 时 nupkg 缺损。
  # postFixup 在 normalization 之后执行，用 dotnet pack 重新打包并覆盖两处。
  postFixup = ''
    dotnet pack src/csharp/CustomData.csproj \
      -p:ContinuousIntegrationBuild=true \
      -p:Deterministic=true \
      --output "$out/share/nuget/source" \
      --configuration Release \
      --no-restore \
      --no-build
    mkdir -p "$out/share/nuget/source/masterpilot.customdata.core/$version"
    cp "$out/share/nuget/source/MasterPilot.CustomData.Core.$version.nupkg" \
       "$out/share/nuget/source/masterpilot.customdata.core/$version/masterpilot.customdata.core.0.1.0.nupkg"
  '';

  meta = {
    description = "MasterPilot CustomData - C# core transport layer";
  };
}
