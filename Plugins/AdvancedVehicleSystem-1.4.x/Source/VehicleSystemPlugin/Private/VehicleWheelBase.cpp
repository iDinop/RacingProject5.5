// Copyright 2019-2024 Overtorque Creations LLC. All Rights Reserved.
// Unauthorized copying of this file, via any medium is strictly prohibited

#include "VehicleWheelBase.h"

#include "AVS_DEBUG.h"
#include "VehicleSystemFunctions.h"
#include "Components/SphereComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "VehicleSystemPlugin/VehicleSystemPlugin.h"

UVehicleWheelBase::UVehicleWheelBase(): WheelStaticMesh(nullptr), WheelMeshComponent(nullptr)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVehicleWheelBase::BeginPlay()
{
	Super::BeginPlay();

	// Initialize wheel config
	UpdateLocalTransformCache();
	WheelConfig.CalculateConstants();
	UpdateWheelRadius();
}

void UVehicleWheelBase::UpdateWheelRadius()
{
	if( !IsValid(WheelMeshComponent) ) return;
	
	if( WheelConfig.AutoWheelRadius )
	{
		USphereComponent* WheelSphere = Cast<USphereComponent>(WheelMeshComponent);
		if( IsValid(WheelSphere) ) return;
		
		WheelConfig.WheelRadius = UVehicleSystemFunctions::GetMeshRadius(WheelMeshComponent);
		if( WheelConfig.WheelRadius <= 0.0f ) WheelConfig.WheelRadius = 30.0f;
	}
}

void UVehicleWheelBase::UpdateLocalTransformCache()
{
	UPrimitiveComponent* VehicleMesh = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	if( !IsValid(VehicleMesh) )
		return;
	
	WheelConfig.WheelLocalTransform = GetComponentTransform().GetRelativeTransform(VehicleMesh->GetBodyInstance()->GetUnrealWorldTransform());
}

void UVehicleWheelBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if( WheelConfig.WheelMode != EWheelMode::Raycast )
		return;

	if( !IsValid(WheelMeshComponent) || !GetIsAttached() || !GetIsSimulatingSuspension() )
		return;

	WheelConfig.isLocked = isLocked; // Copy isLocked into wheel config every frame, TODO not ideal should be changed later

	if( GetHasContact() && !PassiveMode )
	{
		CurAngVel = WheelData.AngularVelocity;
	}
	else
	{
		if( isLocked )
		{
			CurAngVel = 0.0f;
		}
		else
		{
			if( PassiveMode ) TargetAngVel = 0.0f;
			bool isAccelerating = (FMath::Abs(TargetAngVel) > FMath::Abs(CurAngVel));
			float DriveInterpSpeed = isAccelerating ? 2.0f : 1.0f;
			float FinalTargetAV = WheelConfig.IsDrivingWheel ? TargetAngVel : 0.0f;
			float InterpSpeed = WheelConfig.IsDrivingWheel ? DriveInterpSpeed : 0.2f;
			CurAngVel = FMath::FInterpTo(CurAngVel, FinalTargetAV, DeltaTime, InterpSpeed);

			#if !UE_BUILD_SHIPPING
			if( !FMath::IsFinite(TargetAngVel) || !FMath::IsFinite(InterpSpeed) ) UE_LOG(LogAVS, Error, TEXT("Bad value detected during calculation! TargetAngVel: %f, InterpSpeed: %f"), TargetAngVel, InterpSpeed);
			#endif
		}
	}
	
	constexpr float RadsToDegreesPerSecond = (180.0f / PI);
	const float SpringStart = WheelConfig.SpringLength*0.5f;
	
	// Add delta rotation
	WheelRotation = UKismetMathLibrary::ComposeRotators(WheelRotation, FRotator(CurAngVel * RadsToDegreesPerSecond * -1.0f * DeltaTime, 0.0f, 0.0f));
	
	FVector NewLoc = FVector(0,0, FMath::Min(SpringStart + (WheelData.CurrentSpringLength * -1.0f), SpringStart) );
	FRotator NewRot = UKismetMathLibrary::ComposeRotators(WheelRotation, FRotator(0.0f, GetSteeringAngle(), 0.0f));
	WheelMeshComponent->SetRelativeLocationAndRotation(NewLoc, NewRot);

	if( PassiveMode && FMath::Abs(CurAngVel) <= 0.01f )
	{
		SetComponentTickEnabled(false);
	}
}

void UVehicleWheelBase::SetWheelMode_Implementation(EWheelMode NewMode)
{
	if( !IsValid(WheelMeshComponent) )
		return;
	
	WheelConfig.WheelMode = NewMode;
	ResetWheelCollisions();
}

void UVehicleWheelBase::ResetWheelCollisions()
{
	if( WheelConfig.WheelMode == EWheelMode::Physics )
	{
		WheelMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else // Raycast
	{
		WheelMeshComponent->SetSimulatePhysics(false);
		WheelMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WheelMeshComponent->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
}

float UVehicleWheelBase::GetWheelAngVelInRadians()
{
	if(GetWheelMode() == EWheelMode::Physics)
	{
		if( !IsValid(WheelMeshComponent) )
			return 0.0f;

		return WheelMeshComponent->GetPhysicsAngularVelocityInRadians().Length(); // TODO: Isolate the Y axis
	}
	return WheelData.AngularVelocity; // Return from raycast data
}

FVector UVehicleWheelBase::GetWheelVelocity(bool Local)
{
	EWheelMode CurWheelMode = GetWheelMode();
	FTransform WheelWorldTransform = WheelMeshComponent->GetComponentTransform();;
	FVector WheelVelocityWorld;
	
	if( CurWheelMode == EWheelMode::Physics )
	{
		WheelVelocityWorld = WheelMeshComponent->GetPhysicsLinearVelocity();
	}
	else
	{
		UPrimitiveComponent* VehicleMesh = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
		if( !IsValid(VehicleMesh) )
			return FVector::ZeroVector;
		
		WheelVelocityWorld = VehicleMesh->GetPhysicsLinearVelocityAtPoint( WheelWorldTransform.GetLocation() );
	}

	if( Local )
	{
		return WheelWorldTransform.Inverse().TransformVectorNoScale(WheelVelocityWorld);
	}

	return WheelVelocityWorld;
}

// TODO: Implement proper wheel locking by brake torque
/*bool UVehicleWheelBase::LockedByBrake(float BrakeTorque, float DeltaTime)
{
	if( BrakeTorque == 0.0f ) return false;
	if( WheelData.AngularVelocity == 0.0f ) return true;

	const float SignBefore = FMath::Sign(WheelData.AngularVelocity);
	float AngVel = WheelData.AngularVelocity + (-SignBefore * FMath::Abs(BrakeTorque) / WheelConfig.Inertia * DeltaTime);
	const float SignAfter = FMath::Sign(AngVel);

	UAVS_DEBUG::SCREEN(EDebugCategory::PHYSICS, 0.04f, TXT("WheelData.AngularVelocity: %f, Locked: %d", WheelData.AngularVelocity, (SignAfter != SignBefore)));

	if( SignAfter != SignBefore ) return true;
	return false;
}*/

