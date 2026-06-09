{
  vendor-src,

  lib,
  stdenv,
  makeWrapper,

  dotnet-sdk,
  fetchNuGet,
  buildDotnetModule,
  buildDotnetGlobalTool,
  systemToDotnetRid,

  patchelf,
  libclang,
  icu,
  openssl,
  zlib 
}:

let
  pname = "clangsharp-generator";
  version = "21.1.8.2";
  runtimeId = systemToDotnetRid stdenv.hostPlatform.system;
  nugetDeps = ./nuget-deps.json;

  dllExt = stdenv.hostPlatform.extensions.sharedLibrary;

  vendorModule = buildDotnetModule {
    pname = "${pname}-vendor";
    src = vendor-src;
    projectFile = "vendor.csproj";
    inherit version nugetDeps;
    # 不填runtimeId, 获取所有依赖的hash
  };
  targetPname = "ClangSharpPInvokeGenerator.${runtimeId}";
  platformNupkg = lib.findFirst
    (pkg: pkg.pname == targetPname && pkg.version == version)
    (throw "Unsupported platform in nuget-deps: ${targetPname}")
    vendorModule.nugetDeps;

in

stdenv.mkDerivation {
  inherit pname version;

  dontBuild = true;
  dontConfigure = true;

  src = platformNupkg;

  nativeBuildInputs = [ makeWrapper patchelf ];

  installPhase = ''
    runHook preInstall

    mkdir -p $out/bin $out/lib

    assetsPath=$(ls -d share/nuget/packages/*/*/tools/any/${runtimeId})    

    cp $assetsPath/libClangSharp${dllExt} $out/lib/
    cp $assetsPath/ClangSharpPInvokeGenerator $out/bin/

    ${lib.optionalString stdenv.isLinux ''
      runtimeLibs="${lib.makeLibraryPath [ stdenv.cc.cc.lib icu openssl zlib ]}:$out/lib"

      patchelf \
        --set-interpreter "$(cat $NIX_CC/nix-support/dynamic-linker)" \
        --set-rpath "$runtimeLibs" \
        $out/bin/ClangSharpPInvokeGenerator
    ''}

    makeWrapper $out/bin/ClangSharpPInvokeGenerator $out/bin/${pname} \
      --prefix LD_LIBRARY_PATH : "${lib.makeLibraryPath [ libclang.lib ]}:$out/lib" \
      --prefix DYLD_LIBRARY_PATH : "${lib.makeLibraryPath [ libclang.lib ]}:$out/lib"

    runHook postInstall
  '';

  meta = with lib; {
    description = "ClangSharp P/Invoke Generator natively compiled and cleanly wrapped";
    homepage = "https://github.com/dotnet/ClangSharp";
    license = licenses.mit;
    platforms = platforms.unix;
  };

  passthru = {
    inherit vendorModule;
    inherit (vendorModule) fetch-deps;
  };
}