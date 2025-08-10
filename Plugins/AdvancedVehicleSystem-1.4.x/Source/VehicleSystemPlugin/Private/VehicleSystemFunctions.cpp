// Copyright 2019-2024 Overtorque Creations LLC. All Rights Reserved.
// Unauthorized copying of this file, via any medium is strictly prohibited

#include "VehicleSystemFunctions.h"
#include <Runtime/Engine/Classes/Engine/Engine.h>

#include "AVS_DEBUG.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interfaces/IPluginManager.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

UVehicleSystemFunctions::UVehicleSystemFunctions(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

FString UVehicleSystemFunctions::GetPluginVersion()
{
	FString PluginName = "VehicleSystemPlugin";
 
	IPluginManager& PluginManager = IPluginManager::Get();
	TArray<TSharedRef<IPlugin>> Plugins = PluginManager.GetDiscoveredPlugins();
	for(const TSharedRef<IPlugin>& Plugin: Plugins)
	{
		if (Plugin->GetName() == PluginName)
		{
			const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
			FString Version = Descriptor.VersionName;
			return Version;
		}
	}
	return "Error";
}

// This is necessary because the function provided by the engine doesn't include the bone name
void UVehicleSystemFunctions::SetLinearDamping(UPrimitiveComponent* target, float InDamping, FName BoneName)
{
	if(target)
	{
		FBodyInstance* BI = target->GetBodyInstance(BoneName);
		if (BI)
		{
			BI->LinearDamping = InDamping;
			BI->UpdateDampingProperties();
		}
	}
}

// This is necessary because the function provided by the engine doesn't include the bone name
void UVehicleSystemFunctions::SetAngularDamping(UPrimitiveComponent* target, float InDamping, FName BoneName)
{
	if(target)
	{
		FBodyInstance* BI = target->GetBodyInstance(BoneName);
		if (BI)
		{
			BI->AngularDamping = InDamping;
			BI->UpdateDampingProperties();
		}
	}
}

float UVehicleSystemFunctions::GetMeshDiameter(UPrimitiveComponent* target, FName BoneName)
{
	return GetMeshRadius(target, BoneName) * 2;
}

float UVehicleSystemFunctions::GetMeshRadius(UPrimitiveComponent* target, FName BoneName)
{
	if( IsValid(target) )
	{
		// Handle UStaticMeshComponent type
		if( UStaticMeshComponent* TargetStatic = Cast<UStaticMeshComponent>(target) )
		{
			FBoxSphereBounds MyBounds = TargetStatic->GetStaticMesh()->GetBounds();
			return MyBounds.BoxExtent.Z * TargetStatic->GetComponentScale().Z;
		}

		// Handle UShapeComponent type
		if( UShapeComponent* TargetShape = Cast<UShapeComponent>(target) )
		{
			return TargetShape->Bounds.BoxExtent.Z * TargetShape->GetComponentScale().Z;
		}

		// Fallback to BodyInstance bounds
		FBodyInstance* BI = target->GetBodyInstance(BoneName);
		if (BI)
		{
			FBoxSphereBounds MyBounds = BI->GetBodyBounds();
			return MyBounds.BoxExtent.Z;
		}
	}

	return 0.0f;
}

FVector UVehicleSystemFunctions::GetMeshCenterOfMass(UPrimitiveComponent* target, FName BoneName)
{
	FBodyInstance* targetBI = target->GetBodyInstance(BoneName);
	if (targetBI)
	{
		return targetBI->GetMassSpaceLocal().GetLocation();
	}

	return FVector::ZeroVector;
}

void UVehicleSystemFunctions::PrintToScreenWithTag(const FString& InString, FLinearColor TextColor, float Duration, int Tag)
{
	GEngine->AddOnScreenDebugMessage(Tag, Duration, TextColor.ToFColor(true), InString);
}

//TODO: Change these functions into a single output ENUM, GetCurrentWorldType
bool UVehicleSystemFunctions::RunningInEditor_World(UObject* WorldContextObject)
{
	if(!WorldContextObject) return false;
	
	//using a context object to get the world
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if(!World) return false;
	
	return (World->WorldType == EWorldType::Editor );
}

bool UVehicleSystemFunctions::RunningInPIE_World(UObject* WorldContextObject)
{
	if(!WorldContextObject) return false;
	
	//using a context object to get the world
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if(!World) return false;
	
	return (World->WorldType == EWorldType::PIE );
}

bool UVehicleSystemFunctions::RunningInGame_World(UObject* WorldContextObject)
{
	if(!WorldContextObject) return false;
	
	//using a context object to get the world
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if(!World) return false;
	
	return (World->WorldType == EWorldType::Game );
}

double UVehicleSystemFunctions::LinearSpeedToRads(double cm_per_sec, float Radius)
{
	#if !UE_BUILD_SHIPPING
	if( Radius == 0.0f )
	{
		UE_LOG(LogAVS, Error, TEXT("Attempted division by zero in LinearSpeedToRads."));
		return 0.0;
	}
	#endif
	return cm_per_sec / Radius;
}

//Chaos physics thread force functions

FTransform UVehicleSystemFunctions::AVS_GetChaosTransform(UPrimitiveComponent* target)
{
	if(IsValid(target))
	{
		if(const FBodyInstance* BodyInstance = target->GetBodyInstance())
		{
			if(auto Handle = BodyInstance->ActorHandle)
			{
				if(Chaos::FRigidBodyHandle_Internal* RigidHandle = Handle->GetPhysicsThreadAPI())
				{
					const Chaos::FRigidTransform3 WorldCOM = Chaos::FParticleUtilitiesGT::GetActorWorldTransform(RigidHandle);
					return WorldCOM;
				}
			}
		}
	}
	return FTransform();
}


void UVehicleSystemFunctions::AVS_ChaosAddForce(UPrimitiveComponent* target, FVector Force, bool bAccelChange = false)
{
	if(!target)
		return;
	
	if(const FBodyInstance* BodyInstance = target->GetBodyInstance())
	{
		if(auto Handle = BodyInstance->ActorHandle)
		{
			if(Chaos::FRigidBodyHandle_Internal* RigidHandle = Handle->GetPhysicsThreadAPI())
			{
				if(bAccelChange)
				{
					const float RigidMass = RigidHandle->M();
					const Chaos::FVec3 Acceleration = Force * RigidMass;
					RigidHandle->AddForce(Acceleration, false);
				}
				else
				{
					RigidHandle->AddForce(Force, false);
				}
			}
		}
	}
}

void UVehicleSystemFunctions::AVS_ChaosAddForceAtLocation(UPrimitiveComponent* target, FVector Location, FVector Force)
{
	if(!target)
		return;
	
	if(const FBodyInstance* BodyInstance = target->GetBodyInstance())
	{
		if(auto Handle = BodyInstance->ActorHandle)
		{
			if(Chaos::FRigidBodyHandle_Internal* RigidHandle = Handle->GetPhysicsThreadAPI())
			{
				const Chaos::FVec3 WorldCOM = Chaos::FParticleUtilitiesGT::GetCoMWorldPosition(RigidHandle);
				const Chaos::FVec3 WorldTorque = Chaos::FVec3::CrossProduct(Location - WorldCOM, Force);
				RigidHandle->AddForce(Force, false);
				RigidHandle->AddTorque(WorldTorque, false);
			}
		}
	}
}

void UVehicleSystemFunctions::AVS_ChaosAddTorque(UPrimitiveComponent* target, FVector Torque, bool bAccelChange)
{
	if(!target)
		return;
	
	if(const FBodyInstance* BodyInstance = target->GetBodyInstance())
	{
		if(auto Handle = BodyInstance->ActorHandle)
		{
			if(Chaos::FRigidBodyHandle_Internal* RigidHandle = Handle->GetPhysicsThreadAPI())
			{
				if(bAccelChange)
				{
					RigidHandle->AddTorque(Chaos::FParticleUtilitiesXR::GetWorldInertia(RigidHandle) * Torque, false);
				}
				else
				{
					RigidHandle->AddTorque(Torque, false);
				}
			}
		}
	}
}

void UVehicleSystemFunctions::AVS_ChaosAddWheelTorque(UPrimitiveComponent* target, float Torque, bool bAccelChange)
{
	if(!target)
		return;
	
	if(const FBodyInstance* BodyInstance = target->GetBodyInstance())
	{
		if(auto Handle = BodyInstance->ActorHandle)
		{
			if(Chaos::FRigidBodyHandle_Internal* RigidHandle = Handle->GetPhysicsThreadAPI())
			{
				FTransform TargetWorldTransform(RigidHandle->R(), RigidHandle->X());
				Chaos::FVec3 TorqueVector = Torque * TargetWorldTransform.GetUnitAxis(EAxis::Y);
				if(bAccelChange)
				{
					RigidHandle->AddTorque(Chaos::FParticleUtilitiesXR::GetWorldInertia(RigidHandle) * TorqueVector, false);
				}
				else
				{
					RigidHandle->AddTorque(TorqueVector, false);
				}
			}
		}
	}
}

void UVehicleSystemFunctions::AVS_ChaosSetWheelAngularVelocity(UPrimitiveComponent* target, float AngVel)
{
	if( !target )
		return;

	if( const FBodyInstance* BodyInstance = target->GetBodyInstance() )
	{
		if( auto Handle = BodyInstance->ActorHandle )
		{
			if( Chaos::FRigidBodyHandle_Internal* RigidHandle = Handle->GetPhysicsThreadAPI() )
			{
				FTransform TargetWorldTransform(RigidHandle->R(), RigidHandle->X());
				Chaos::FVec3 AngVelVector = AngVel * TargetWorldTransform.GetUnitAxis(EAxis::Y);
				RigidHandle->SetW(AngVelVector);
			}
		}
	}
}

void UVehicleSystemFunctions::AVS_SetWheelAngularVelocity(UPrimitiveComponent* target, float AngVel)
{
	if( !target )
		return;

	FVector AngVelVector = AngVel * target->GetComponentTransform().GetUnitAxis(EAxis::Y);
	target->SetPhysicsAngularVelocityInRadians(AngVelVector);
}

void UVehicleSystemFunctions::AVS_ChaosBrakes(UPrimitiveComponent* target, float BrakeTorque, float ChaosDelta)
{
	if(!target)
		return;
	
	if(const FBodyInstance* BodyInstance = target->GetBodyInstance())
	{
		if(auto Handle = BodyInstance->ActorHandle)
		{
			if(Chaos::FRigidBodyHandle_Internal* RigidHandle = Handle->GetPhysicsThreadAPI())
			{
				FTransform TargetWorldTransform(RigidHandle->R(), RigidHandle->X());

				Chaos::FVec3 AngVel = RigidHandle->W();
				Chaos::FVec3 FullStopTorque = (AngVel / ChaosDelta)*(-1.0f);

				Chaos::FVec3 FullStopTorqueLocal = TargetWorldTransform.InverseTransformVectorNoScale(FullStopTorque); // Convert to local space
				FullStopTorqueLocal *= Chaos::FVec3::RightVector; // Isolate the Y rotation

				Chaos::FVec3 FullStopTorqueY = RigidHandle->R().RotateVector(FullStopTorqueLocal); // Convert back to world space
				Chaos::FVec3 FinalBrakeForce = FullStopTorqueY.GetClampedToMaxSize(BrakeTorque); // Clamp to input brake force, if full stop exceeds brake force, wheel will only be slowed
				RigidHandle->AddTorque(Chaos::FParticleUtilitiesXR::GetWorldInertia(RigidHandle) * FinalBrakeForce, false);
			}
		}
	}
}

FVector UVehicleSystemFunctions::AVS_ChaosGetVelocityAtLocation(UPrimitiveComponent* target, FVector Location)
{
	if(const FBodyInstance* BodyInstance = target->GetBodyInstance())
	{
		if(!BodyInstance)
			return FVector::ZeroVector;

		if(auto Handle = BodyInstance->ActorHandle)
		{
			if(Chaos::FRigidBodyHandle_Internal* RigidHandle = Handle->GetPhysicsThreadAPI())
			{
				const bool bIsRigid = RigidHandle->CanTreatAsRigid();
				const Chaos::FVec3 COM = bIsRigid ? Chaos::FParticleUtilitiesGT::GetCoMWorldPosition(RigidHandle) : static_cast<Chaos::FVec3>(Chaos::FParticleUtilitiesGT::GetActorWorldTransform(RigidHandle).GetTranslation());
				const Chaos::FVec3 Diff = Location - COM;
				return RigidHandle->V() - Chaos::FVec3::CrossProduct(Diff, RigidHandle->W());
			}
		}
	}
	return FVector::ZeroVector;
}
