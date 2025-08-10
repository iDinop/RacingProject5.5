// Copyright 2019-2024 Overtorque Creations LLC. All Rights Reserved.
// Unauthorized copying of this file, via any medium is strictly prohibited

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Runtime/Core/Public/Misc/EngineVersion.h"
#include "VehicleSystemFunctions.generated.h"

/* 
*	Function library class.
*	Each function in it is expected to be static and represents blueprint node that can be called in any blueprint.
*
*	When declaring function you can define metadata for the node. Key function specifiers will be BlueprintPure and BlueprintCallable.
*	BlueprintPure - means the function does not affect the owning object in any way and thus creates a node without Exec pins.
*	BlueprintCallable - makes a function which can be executed in Blueprints - Thus it has Exec pins.
*	DisplayName - full name of the node, shown when you mouse over the node and in the blueprint drop down menu.
*				Its lets you name the node using characters not allowed in C++ function names.
*	CompactNodeTitle - the word(s) that appear on the node.
*	Keywords -	the list of keywords that helps you to find node when you search for it using Blueprint drop-down menu. 
*				Good example is "Print String" node which you can find also by using keyword "log".
*	Category -	the category your node will be under in the Blueprint drop-down menu.
*
*	For more info on custom blueprint nodes visit documentation:
*	https://wiki.unrealengine.com/Custom_Blueprint_Node_Creation
*/

UCLASS()
class UVehicleSystemFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Get the version of Advanced Vehicle System you are running, this function is expensive and not meant to be run on tick! */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin")
	static FString GetPluginVersion();

	/** AVS Simple Get Unreal Engine Version */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin")
	static void GetUnrealEngineVersion(int& Major, int& Minor, int& Patch)
	{
		FEngineVersion EngineVer = FEngineVersion::Current();
		Major = EngineVer.GetMajor();
		Minor = EngineVer.GetMinor();
		Patch = EngineVer.GetPatch();
	};

	// less accurate but faster distance calculation (Manhattan distance)
	static float FastDist(const FVector& A, const FVector& B)
	{
		return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y) + FMath::Abs(A.Z - B.Z); // Manhattan distance
	}

	/** Sets the linear damping of this component on the named bone, or entire primitive if not specified */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin")
	static void SetLinearDamping(UPrimitiveComponent* target, float InDamping, FName BoneName = NAME_None);

	/** Sets the angular damping of this component on the named bone, or entire primitive if not specified */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin")
	static void SetAngularDamping(UPrimitiveComponent* target, float InDamping, FName BoneName = NAME_None);

	/** Get the height of this body. */
	UFUNCTION(BlueprintPure, Category = "VehicleSystemPlugin")
	static float GetMeshDiameter(UPrimitiveComponent* target, FName BoneName = NAME_None);

	/** Get the half height of this body. */
	UFUNCTION(BlueprintPure, Category = "VehicleSystemPlugin")
	static float GetMeshRadius(UPrimitiveComponent* target, FName BoneName = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin")
	static float GetWheelInertia(UPrimitiveComponent* target, float MassKg, float RadiusCm)
	{
		float Radius = RadiusCm * 0.01f;
		return 0.4f * MassKg * (Radius*Radius);
	}

	/** Get the Center of Mass for a body (In Relative Space) */
	UFUNCTION(BlueprintPure, Category = "VehicleSystemPlugin", meta=(Keywords = "COM"))
	static FVector GetMeshCenterOfMass(UPrimitiveComponent* target, FName BoneName = NAME_None);

	/** Print to screen with updatable text */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin")
	static void PrintToScreenWithTag(const FString& InString, FLinearColor TextColor = FLinearColor::Green, float Duration = 1.0f, int Tag = 1);

	/** Is this game logic running in the Editor world? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VehicleSystemPlugin", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
    static bool RunningInEditor_World(UObject* WorldContextObject);

	/** Is this game logic running in the PIE world? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VehicleSystemPlugin", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
    static bool RunningInPIE_World(UObject* WorldContextObject);

	/** Is this game logic running in an actual independent game instance? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VehicleSystemPlugin", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
    static bool RunningInGame_World(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VehicleSystemPlugin")
	static double LinearSpeedToRads(double cm_per_sec, float Radius);

	//Chaos Physics Functions

	/** For use on the chaos physics thread only */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin - Chaos Functions")
	static FTransform AVS_GetChaosTransform(UPrimitiveComponent* target);
	
	/** For use on the chaos physics thread only */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin - Chaos Functions")
	static void AVS_ChaosAddForce(UPrimitiveComponent* target, FVector Force, bool bAccelChange);
	
	/** For use on the chaos physics thread only */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin - Chaos Functions")
	static void AVS_ChaosAddForceAtLocation(UPrimitiveComponent* target, FVector Location, FVector Force);

	/** For use on the chaos physics thread only */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin - Chaos Functions")
	static void AVS_ChaosAddTorque(UPrimitiveComponent* target, FVector Torque, bool bAccelChange);

	/** For use on the chaos physics thread :: Adds braking torque isolated to the primitives local Y axis */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin - Chaos Functions", meta=(NotBlueprintThreadSafe))
	static void AVS_ChaosBrakes(UPrimitiveComponent* target, float BrakeTorque, float ChaosDelta);

	/** For use on the chaos physics thread :: Supplies torque around the targets Y axis */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin - Chaos Functions")
	static void AVS_ChaosAddWheelTorque(UPrimitiveComponent* target, float Torque, bool bAccelChange);

	/** For use on the chaos physics thread :: Sets angular velocity (rad/s) around the targets Y axis */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin - Chaos Functions")
	static void AVS_ChaosSetWheelAngularVelocity(UPrimitiveComponent* target, float AngVel);

	/** For use on the chaos physics thread :: Sets angular velocity (rad/s) around the targets Y axis */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin - Chaos Functions")
	static void AVS_SetWheelAngularVelocity(UPrimitiveComponent* target, float AngVel);

	/** For use on the chaos physics thread only */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin - Chaos Functions")
	static FVector AVS_ChaosGetVelocityAtLocation(UPrimitiveComponent* Component, FVector Location);
};
