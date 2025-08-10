// Copyright 2019-2024 Overtorque Creations LLC. All Rights Reserved.
// Unauthorized copying of this file, via any medium is strictly prohibited

#pragma once

#include "VehicleWheelBase.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Runtime/Launch/Resources/Version.h"

struct FVehiclePhysicsPhysicsInput : public Chaos::FSimCallbackInput
{
	TWeakObjectPtr<UWorld> World;
	
	TWeakObjectPtr<APawn> VehicleActor; //Has to be a pawn to avoid circular references
	UPrimitiveComponent* VehicleMeshPrim;
	float VehicleMass = 0.0f;

	FAVS_Inputs VehicleInputs;
	
	TArray<FAVS1_Wheel_Config> Wheels;

	void Reset() //Required
	{
		VehicleActor = nullptr;
		VehicleMeshPrim = nullptr;
		VehicleMass = 0.0f;
		Wheels.Reset();
		World.Reset();
	}
}; 
struct FVehiclePhysicsPhysicsOutput : public Chaos::FSimCallbackOutput
{
	float ChaosDeltaTime = 0.0f;
	
	// Raycast wheel data
	TArray<FHitResult> DebugTraces; // Raw trace data generated on physics thread
	TArray<FDebugForce> DebugForces; // Forces applied to the vehicle
	TArray<FString> DebugTexts;
	
	TArray<FAVS1_Wheel_Output> WheelOutputs;
	
	void Reset() //Required
	{
		ChaosDeltaTime = 0.0f;
		DebugTraces.Empty();
		DebugForces.Empty();
		DebugTexts.Empty();
		WheelOutputs.Empty();
	}
};

// Unreal 5.1+ Physics Callback
class FVehiclePhysicsCallback : public Chaos::TSimCallbackObject<FVehiclePhysicsPhysicsInput, FVehiclePhysicsPhysicsOutput, Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::ContactModification>
{
public:
	Chaos::FSingleParticlePhysicsProxy* VehicleMesh;
	TArray<Chaos::FSingleParticlePhysicsProxy*> WheelMeshes;
private:
	virtual void OnPreSimulate_Internal() override;
	virtual void OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifier) override;
};