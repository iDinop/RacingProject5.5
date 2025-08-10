// Copyright 2019-2024 Overtorque Creations LLC. All Rights Reserved.
// Unauthorized copying of this file, via any medium is strictly prohibited

using UnrealBuildTool;

public class VehicleSystemPlugin : ModuleRules
{
	public VehicleSystemPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		//IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", });
		PrivateDependencyModuleNames.AddRange(new string[] { "Projects", "CoreUObject", "Engine", "Chaos", });

		//Required for Chaos physics callbacks
		SetupModulePhysicsSupport(Target);
	}
}
