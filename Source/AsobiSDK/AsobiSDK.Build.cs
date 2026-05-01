using System.IO;
using UnrealBuildTool;


public class AsobiSDK : ModuleRules
{
	public AsobiSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"Json",
			"JsonUtilities",
			"Projects",
			"WebSockets"
		});

		// AsobiCore is a plain C++17 library that owns the engine-agnostic
		// protocol identity (wire-`type` -> EventId mapping). Its sources
		// build and unit-test on stock CI (ubuntu-24.04 + g++/clang +
		// doctest) so the dispatch gate runs on every PR without
		// requiring an Unreal Engine install.
		//
		// AsobiCore has zero UE dependencies and is a single small .cpp,
		// so we compile it directly into this UE module via the thin
		// wrapper at Private/AsobiCoreImpl.cpp (which #includes the real
		// AsobiCore/Private/Protocol.cpp). This avoids the per-platform
		// prebuilt + CMake-as-pre-build-step that PublicAdditionalLibraries
		// would require, and keeps UBT in charge of toolchain differences.
		string CoreRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "AsobiCore"));
		PublicIncludePaths.Add(Path.Combine(CoreRoot, "Public"));
	}
}
