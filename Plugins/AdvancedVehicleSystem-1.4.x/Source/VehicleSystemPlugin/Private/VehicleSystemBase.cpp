// Copyright 2019-2024 Overtorque Creations LLC. All Rights Reserved.
// Unauthorized copying of this file, via any medium is strictly prohibited

#define DEBUG(format, value) FString::Printf(TEXT(format), value)

#include "VehicleSystemBase.h"

#include "AVS_DEBUG.h"
#include "PBDRigidsSolver.h"
#include "TimerManager.h"
#include "VehicleSystemFunctions.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Runtime/Engine/Classes/Camera/PlayerCameraManager.h"
#include "Runtime/Engine/Classes/GameFramework/PlayerController.h"

AVehicleSystemBase::AVehicleSystemBase()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	VehicleMesh = CreateDefaultSubobject<UStaticMeshComponent>("VehicleMesh");
	SetRootComponent(VehicleMesh);

	ReplicateMovement = true;
	SyncLocation = true;
	SyncRotation = true;
	NetSendRate = 0.05f;
	NetTimeBehind = 0.15f;
	NetLerpStart = 0.35f;
	NetPositionTolerance = 0.1f;
	NetSmoothing = 10.0f;

	// Init steering speed curve
	FRichCurve* SteeringCurveData = SteeringFalloffCurve.GetRichCurve();
	SteeringCurveData->AddKey(0.f, 1.f);
	SteeringCurveData->AddKey(20.f, 0.8f);
	SteeringCurveData->AddKey(60.f, 0.4f);
	SteeringCurveData->AddKey(120.f, 0.3f);
}

//Replicated Variables
void AVehicleSystemBase::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AVehicleSystemBase, RestState);
}

void AVehicleSystemBase::BeginPlay()
{
	Super::BeginPlay();
	SetReplicationTimer(ReplicateMovement);
	RegisterPhysicsCallback();

	UpdateInternalWheelArray();
}

void AVehicleSystemBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if(IsPhysicsCallbackRegistered())
	{
		if (UWorld* World = GetWorld())
		{
			if (FPhysScene* PhysScene = World->GetPhysicsScene())
			{
				PhysScene->GetSolver()->UnregisterAndFreeSimCallbackObject_External(PhysicsThreadCallback);
				PhysicsThreadCallback = nullptr;
			}
		}
	}
}

void AVehicleSystemBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if(GetLocalRole() == ROLE_Authority)
	{
		Multicast_ChangedOwner();
	}
	ClearQueue();
}

void AVehicleSystemBase::UnPossessed()
{
	Super::UnPossessed();
	if(GetLocalRole() == ROLE_Authority)
	{
		Multicast_ChangedOwner();
	}
	ClearQueue();
}

void AVehicleSystemBase::PassiveStateChanged_Implementation(bool NewPassiveState)
{
	for( UVehicleWheelBase* Wheel : VehicleWheels )
	{
		Wheel->SetPassiveMode(PassiveMode);
	}
}

// Sets IsVehicleAtRest to true if the vehicle is within the velocity threshold for 3 seconds
void AVehicleSystemBase::DetermineLocalRestState()
{
	bool WithinRestThreshold = VehicleMesh->GetPhysicsLinearVelocity().Size() <= RestVelocityThreshold; // Vehicle is moving slow enough to be considered not moving (resting)
	if (WithinRestThreshold) // Meets resting requirements
	{
		if (!LocalVehicleAtRest) // Variable not resting yet
		{
			RestTimer += TickDeltaTime; // Increment rest timer
			if (RestTimer >= 3.0f) // Resting conditions have been met for at least 3 seconds
			{
				LocalVehicleAtRest = true;
			}
		}
		else
		{
			RestTimer = 0.0f; // Variable is at rest, reset timer
		}
	}
	else // Does not meet resting requirements
	{
		RestTimer = 0.0f; // Not resting, Reset rest timer
		LocalVehicleAtRest = false; // Not resting, reset variable
	}
}

// We are using TickActor to gatekeep the tick function
void AVehicleSystemBase::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	if (IsValidChecked(this) && GetWorld())
	{
		TickDeltaTime = DeltaTime;

		// AVS performance checks
		DetermineLocalRestState();
		bool NewPassive = DeterminePassiveState();
		if(NewPassive != PassiveMode)
		{
			PassiveMode = NewPassive;
			PassiveStateChanged(NewPassive);
		}
		AlwaysTick();
		if( PassiveMode && PassiveTickGatekeeping ){ PassiveTick(DeltaTime); return; } // Disallow standard tick when in passive mode
		Super::TickActor(DeltaTime, TickType, ThisTickFunction); // Super will call standard Tick function
	}
}

void AVehicleSystemBase::AlwaysTick()
{
	NetworkTick();

	// Physics Thread
	if( VehicleMesh->IsSimulatingPhysics() )
	{
		// Physics thread updates
		if( !IsPhysicsCallbackRegistered() ) return;

		// Physics Thread Inputs
		FVehiclePhysicsPhysicsInput* PhysicsInput = PhysicsThreadCallback->GetProducerInputData_External();
		PhysicsInput->World = GetWorld();
		PhysicsInput->VehicleActor = this;
		PhysicsInput->VehicleMeshPrim = VehicleMesh;
		PhysicsInput->VehicleMass = VehicleMesh->GetMass();
		PhysicsInput->VehicleInputs = InputsForPhysicsThread;

		PhysicsInput->Wheels.Reset();
		PhysicsInput->Wheels.Reserve(VehicleWheels.Num());

		TArray<UVehicleWheelBase*> SimulatedWheels;
		for( UVehicleWheelBase* Wheel : VehicleWheels )
		{
			if( !IsValid(Wheel) ) continue;
			
			if( Wheel->GetIsAttached() && Wheel->GetIsSimulatingSuspension() )
			{
				PhysicsInput->Wheels.Add(Wheel->WheelConfig);
				SimulatedWheels.Add(Wheel);
			}
		}

		TArray<FString> DebugTexts;
		TArray<FAVS1_Wheel_Output> WheelOutputs;
		// Physics Thread Outputs: Done in a while loop because there can be multiple outputs made between frames
		Chaos::TSimCallbackOutputHandle<FVehiclePhysicsPhysicsOutput> PhysicsOutput;
		while( (PhysicsOutput = PhysicsThreadCallback->PopOutputData_External()) )
		{
			ChaosDeltaTime = PhysicsOutput->ChaosDeltaTime;
			DebugTraces = PhysicsOutput->DebugTraces;
			DebugForces = PhysicsOutput->DebugForces;
			WheelOutputs = PhysicsOutput->WheelOutputs;
			DebugTexts = PhysicsOutput->DebugTexts;
		}

		// Prints all saved debug texts
		for( int32 i = 0; i < DebugTexts.Num(); i++ )
		{
			UVehicleSystemFunctions::PrintToScreenWithTag(DebugTexts[i], FLinearColor::Yellow, 0.1f, i);
		}

		// Wait for the next frame if outputs do not match inputs
		if( WheelOutputs.Num() != SimulatedWheels.Num() ) return;

		// Loop wheel outputs
		for( int Index = 0; Index <= WheelOutputs.Num() - 1; ++Index )
		{
			if( SimulatedWheels.IsValidIndex(Index) )
			{
				SimulatedWheels[Index]->WheelData = WheelOutputs[Index];
			}
		}
	}
}

// Tick that uses minimal resources
void AVehicleSystemBase::PassiveTick(float DeltaTime)
{
	PassiveTickBP(DeltaTime); // Tick blueprints
}

// Standard Unreal Tick
void AVehicleSystemBase::Tick(float DeltaTime)
{
	Super::Tick(TickDeltaTime);

	if( PassiveMode ) return;

	AVS_Tick(TickDeltaTime); // Decoupled blueprint tick

	if( !VehicleMesh->IsAnyRigidBodyAwake() ) // Rigid body is asleep
	{
		// Stop contact wheels from moving because velocity will stop updating
		for( UVehicleWheelBase* Wheel : VehicleWheels )
		{
			if( Wheel->GetHasContact() )
			{
				Wheel->WheelData.AngularVelocity = 0.0f;
			}
		}
	}
}

// AVS Networking Tick
void AVehicleSystemBase::NetworkTick()
{
	NetworkRoles CurrentRole = GetNetworkRole();
	if( CurrentRole != NetworkRoles::Owner )
	{
		if (ReplicateMovement && ShouldSyncWithServer)
		{
			SyncPhysics();
		}
	}

	// Update camera manager for network relevancy
	if( (CurrentRole != NetworkRoles::Server) && IsNetMode(NM_Client) )
	{
		if( APlayerController* PlayerController = Cast<APlayerController>(GetController()) )
		{
			APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
			if( CameraManager && CameraManager->bUseClientSideCameraUpdates )
			{
				CameraManager->bShouldSendClientSideCameraUpdate = true;
			}
		}
	}
}

void AVehicleSystemBase::UpdateInternalWheelArray()
{
	VehicleWheels.Empty();
	TArray<USceneComponent*> ChildArray;
	VehicleMesh->GetChildrenComponents(true, ChildArray);
	ChildArray.Reserve(ChildArray.Num());
	for( int i = 0; i < ChildArray.Num(); ++i )
	{
		UVehicleWheelBase* CurWheel = Cast<UVehicleWheelBase>(ChildArray[i]);
		if( IsValid(CurWheel) )
		{
			VehicleWheels.Add(CurWheel);
		}
	}
}

bool AVehicleSystemBase::IsPhysicsCallbackRegistered()
{
	return PhysicsThreadCallback != nullptr;
}

void AVehicleSystemBase::RegisterPhysicsCallback()
{
	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			PhysicsThreadCallback = PhysScene->GetSolver()->CreateAndRegisterSimCallbackObject_External<FVehiclePhysicsCallback>(); // Unreal 5.1+
			PhysicsThreadCallback->VehicleMesh = VehicleMesh->GetBodyInstance()->GetPhysicsActorHandle();
			VehicleMesh->GetBodyInstance()->SetContactModification(true);
		}
	}
}

void AVehicleSystemBase::WakeWheelsForMovement_Implementation()
{
	// Used in blueprint
}

void AVehicleSystemBase::SetupPlayerInputComponent(UInputComponent *PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AVehicleSystemBase::SetShouldSyncWithServer(bool ShouldSync)
{
	ShouldSyncWithServer = ShouldSync;
	SetReplicationTimer(ShouldSync);
}

void AVehicleSystemBase::SetReplicationTimer(bool Enabled)
{
	if (ReplicateMovement && Enabled)
	{
		GetWorldTimerManager().SetTimer(NetSendTimer, this, &AVehicleSystemBase::NetStateSend, NetSendRate, true);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(NetSendTimer);
		NetworkAtRest = false;
		ClearQueue();
	}
}

void AVehicleSystemBase::NetStateSend()
{
	if (GetNetworkRole() == NetworkRoles::Owner)
	{
		FNetState NewState = CreateNetStateForNow();

		// Only send while not at rest
		if (!LocalVehicleAtRest) // Not at rest
		{
			Server_ReceiveNetState(NewState); // Send moving state
			if (NetworkAtRest) // NetRest is resting but should not be
			{
				FNetState BlankRestState;
				Server_ReceiveRestState(BlankRestState); // Reset NetRest
			}
		}
		else // Is at rest
		{
			// NetworkAtRest is not true but should be, or distance is too different
			const float DistanceThreshold = VehicleMesh->RigidBodyIsAwake() ? 50.0f : 0.5f; // Greater threshold if physics is awake to prevent constantly syncing
			const float MoveDistance = UVehicleSystemFunctions::FastDist(RestState.position, NewState.position);
			if( !NetworkAtRest || MoveDistance > DistanceThreshold )
			{
				UAVS_DEBUG::SCREEN(EDebugCategory::NETWORK, TXT("%s -- Update RestState // Dist %f > DistThreshold %f", *GetFName().ToString(), MoveDistance, DistanceThreshold));
				Server_ReceiveRestState(NewState);
			}
		}

		if (StateQueue.Num() > 0)
		{
			ClearQueue(); //Clear the queue if we are the owner to avoid syncing to old states
		}
	}
}

FNetState AVehicleSystemBase::CreateNetStateForNow()
{
	FNetState newState;
	FTransform primTransform = VehicleMesh->GetComponentToWorld();
	newState.position = primTransform.GetLocation();
	newState.rotation = primTransform.GetRotation().Rotator();
	newState.velocity = VehicleMesh->GetPhysicsLinearVelocity();
	newState.angularVelocity = VehicleMesh->GetPhysicsAngularVelocityInDegrees();
	newState.NetTimestamp = GetNetworkWorldTime();
	return newState;
}

bool AVehicleSystemBase::Server_ReceiveNetState_Validate(FNetState State)
{
	return true;
}
void AVehicleSystemBase::Server_ReceiveNetState_Implementation(FNetState State)
{
	Client_ReceiveNetState(State);
}

bool AVehicleSystemBase::Client_ReceiveNetState_Validate(FNetState State)
{
	return true;
}
void AVehicleSystemBase::Client_ReceiveNetState_Implementation(FNetState State)
{
	if(ShouldSyncWithServer)
	{
		AddStateToQueue(State);
	}
}

bool AVehicleSystemBase::Server_ReceiveRestState_Validate(FNetState State)
{
	return true;
}
void AVehicleSystemBase::Server_ReceiveRestState_Implementation(FNetState State)
{
	RestState = State; // Clients should still receive even when not actively syncing
	if(GetLocalRole() == ROLE_Authority) {OnRep_RestState();} //RepNotify on Server
}

bool AVehicleSystemBase::Multicast_ChangedOwner_Validate()
{
	return true;
}
void AVehicleSystemBase::Multicast_ChangedOwner_Implementation()
{
	ClearQueue();
	OwnerChanged();
}

void AVehicleSystemBase::AddStateToQueue(FNetState StateToAdd)
{
	if (GetNetworkRole() != NetworkRoles::Owner)
	{
		// If we have 10 or more states we are flooded and should drop new states
		if (StateQueue.Num() < 10)
		{
			StateToAdd.NetTimestamp += NetTimeBehind; //Change the timestamp to the future so we can lerp

			if( StateToAdd.NetTimestamp < LastActiveTimestamp )
			{
				return; // This state is late and should be discarded
			}

			if (StateQueue.IsValidIndex(0))
			{
				int8 lastindex = StateQueue.Num() - 1;
				for (int8 i = lastindex; i >= 0; --i)
				{
					if (StateQueue.IsValidIndex(i))
					{
						if( StateQueue[i].NetTimestamp < StateToAdd.NetTimestamp )
						{
							StateQueue.Insert(StateToAdd, i + 1);
							CalculateTimestamps();
							StateToAdd.LocalTimestamp = StateQueue[i + 1].NetTimestamp;
							break;
						}
					}
				}
			}
			else
			{
				StateToAdd.LocalTimestamp = GetLocalWorldTime() + NetTimeBehind;
				StateQueue.Insert(StateToAdd, 0); // If the queue is empty just add it in the first spot
			}
		}
	}
}

void AVehicleSystemBase::ClearQueue()
{
	StateQueue.Empty();
	CreateNewStartState = true;
}

void AVehicleSystemBase::CalculateTimestamps()
{
	int8 lastindex = StateQueue.Num() - 1;
	for (int8 i = 0; i <= lastindex; i++)
	{
		// The first state is always our point of reference and should not change
		// Especially since it could be actively syncing
		if(i != 0)
		{
			if (StateQueue.IsValidIndex(i))
			{
				// Calculate the time difference in the owners times and apply it to our local times
				float timeDifference = StateQueue[i].NetTimestamp - StateQueue[i - 1].NetTimestamp;
				StateQueue[i].LocalTimestamp = StateQueue[i - 1].LocalTimestamp + timeDifference;
			}
		}
	}
}

void AVehicleSystemBase::SyncPhysics()
{
	if( NetworkAtRest )
	{
		SetVehicleLocation(RestState.position, RestState.rotation, true);
		if( StateQueue.Num() > 0 )
		{
			ClearQueue(); // Queue should be empty while resting
		}
		/*if( VehicleMesh->IsAnyRigidBodyAwake() ) // SetVehicleLocation won't wake the physics unless needed
		{
			VehicleMesh->PutRigidBodyToSleep(); // Bug (Chaos): Putting to sleep is causing vehicles to move while asleep while reporting no velocity
			VehicleMesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
			VehicleMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		}*/
		return;
	}

	if (StateQueue.IsValidIndex(0))
	{
		FNetState NextState = StateQueue[0];
		float CurrentTime = GetLocalWorldTime();

		// use physics until we are close enough to this timestamp
		if( CurrentTime >= (NextState.LocalTimestamp - NetLerpStart) )
		{
			if (CreateNewStartState)
			{
				LerpStartState = CreateNetStateForNow();
				CreateNewStartState = false;

					// If our start state is nearly equal to our end state, just skip it
					// This keeps the physics from looking weird when moving slowly, and allows physics to settle
				if (FMath::IsNearlyEqual(LerpStartState.position.X, NextState.position.X, NetPositionTolerance) &&
                    FMath::IsNearlyEqual(LerpStartState.position.Y, NextState.position.Y, NetPositionTolerance) &&
                    FMath::IsNearlyEqual(LerpStartState.position.Z, NextState.position.Z, NetPositionTolerance))
				{
					StateQueue.RemoveAt(0);
					CreateNewStartState = true;
					return;
				}
			}

			LastActiveTimestamp = NextState.NetTimestamp;

			// Lerp To State
			// Our start state may have been created after the lerp start time, so choose whatever is latest
			float lerpBeginTime = LerpStartState.NetTimestamp;
			float lerpPercent = FMath::Clamp(GetPercentBetweenValues(CurrentTime, lerpBeginTime, NextState.LocalTimestamp), 0.0f, 1.0f);
			FVector NewPosition = UKismetMathLibrary::VLerp(LerpStartState.position, NextState.position, lerpPercent);
			FRotator NewRotation = UKismetMathLibrary::RLerp(LerpStartState.rotation, NextState.rotation, lerpPercent, true);
			SetVehicleLocation(NewPosition, NewRotation);

			if( lerpPercent >= 0.99f || lerpBeginTime > NextState.LocalTimestamp )
			{
				ApplyExactNetState(NextState);
				StateQueue.RemoveAt(0);
				CreateNewStartState = true;
			}
		}
	}
}

void AVehicleSystemBase::LerpToNetState(FNetState NextState, float CurrentServerTime)
{
	// Our start state may have been created after the lerp start time, so choose whatever is latest
	float lerpBeginTime = FMath::Max(LerpStartState.NetTimestamp, (NextState.NetTimestamp - NetLerpStart));

	float lerpPercent = FMath::Clamp(GetPercentBetweenValues(CurrentServerTime, lerpBeginTime, NextState.NetTimestamp), 0.0f, 1.0f);

	FVector NewPosition = UKismetMathLibrary::VLerp(LerpStartState.position, NextState.position, lerpPercent);
	FRotator NewRotation = UKismetMathLibrary::RLerp(LerpStartState.rotation, NextState.rotation, lerpPercent, true);
	SetVehicleLocation(NewPosition, NewRotation);
}

void AVehicleSystemBase::ApplyExactNetState(FNetState State)
{
	SetVehicleLocation(State.position, State.rotation);
	VehicleMesh->SetPhysicsLinearVelocity(State.velocity);
	VehicleMesh->SetPhysicsAngularVelocityInDegrees(State.angularVelocity);
}

// Note: SetActorLocationAndRotation is twice as fast as calling SetActorLocation and SetActorRotation separately
// Which is why this is structured this way
void AVehicleSystemBase::SetVehicleLocation(const FVector& InNewPos, const FRotator& InNewRot, bool WakeWheels)
{
	// Teleports to new location instantly if over 3000cm
	float MoveDistance = UVehicleSystemFunctions::FastDist(VehicleMesh->GetComponentLocation(), InNewPos);
	float VehicleVelocity = VehicleMesh->GetComponentVelocity().Length();
	if( MoveDistance <= 3000 )
	{
		float DistanceThreshold = 0.15f;
		if( (NetworkAtRest && RestThresh) || (MoveDistance < DistanceThreshold) ) // Has hit rest threshold at some point
		{
			DistanceThreshold = 10.0f;
			RestThresh = (MoveDistance < DistanceThreshold); // RestThresh only allowed while within threshold
		}
		else
		{
			RestThresh = false;
		}
		
		// Moving fast or moved a lot
		if( (VehicleVelocity > RestVelocityThreshold) || (MoveDistance > DistanceThreshold) ) // Prevent waking physics if we are already close enough
		{
			if( WakeWheels ) WakeWheelsForMovement(); // (Physics Wheel Mode) Physics wheels need woken up to move

			// Move vehicle chassis
			bool IsSimulatingPhysics = VehicleMesh->IsAnyRigidBodyAwake();
			FVector NewPos = IsSimulatingPhysics ? UKismetMathLibrary::VInterpTo(VehicleMesh->GetComponentLocation(), InNewPos, TickDeltaTime, NetSmoothing) : InNewPos;
			FRotator NewRot = IsSimulatingPhysics ? UKismetMathLibrary::RInterpTo(VehicleMesh->GetComponentRotation(), InNewRot, TickDeltaTime, NetSmoothing) : InNewRot;
			if( SyncLocation && SyncRotation )
			{
				SetActorLocationAndRotation(NewPos, NewRot, false, nullptr, TeleportFlagToEnum(!IsSimulatingPhysics));
			}
			else
			{
				if( SyncLocation ) SetActorLocation(NewPos, false, nullptr, TeleportFlagToEnum(!IsSimulatingPhysics));
				if( SyncRotation ) SetActorRotation(NewRot, TeleportFlagToEnum(!IsSimulatingPhysics));
			}
		}
	}
	else
	{
		// Teleport Vehicle chassis
		if( SyncLocation && SyncRotation )
		{
			SetActorLocationAndRotation(InNewPos, InNewRot, false, nullptr, TeleportFlagToEnum(true));
		}
		else
		{
			if( SyncLocation ) SetActorLocation(InNewPos, false, nullptr, TeleportFlagToEnum(true));
			if( SyncRotation ) SetActorRotation(InNewRot, TeleportFlagToEnum(true));
		}
		VehicleMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		VehicleMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

		// Reset the wheel positions
		TeleportWheels();
	}
}

void AVehicleSystemBase::AVS_PhysicsTick(float ChaosDelta, const FVehiclePhysicsPhysicsInput* PhysicsInput, FVehiclePhysicsPhysicsOutput& PhysicsOutput)
{
	using namespace Chaos;

	UWorld* World = PhysicsInput->World.Get(); // only safe to access for scene queries
	if( World == nullptr ) return;
	
	FTransform VehicleBodyTransform = UVehicleSystemFunctions::AVS_GetChaosTransform(PhysicsInput->VehicleMeshPrim);
	TArray<FAVS1_Wheel_Config> Wheels = PhysicsInput->Wheels;
	
	if( WheelStates.Num() != Wheels.Num() ) { WheelStates.SetNum(Wheels.Num()); } // Ensure wheel state array is in sync
	
	// Loop through each wheel
	for( int32 WIndex = 0; WIndex < Wheels.Num(); ++WIndex )
	{
		FAVS1_Wheel_Output WheelOutput; // New output for this wheel
		FAVS1_Wheel_Config WheelConfig = Wheels[WIndex]; // Current configuration from the game thread
		FAVS1_Wheel_State& WheelState = WheelStates[WIndex]; // State data on the physics thread

		FTransform WheelLocalTransform = WheelConfig.WheelLocalTransform;
		if(WheelConfig.IsSteerableWheel) // Steering
		{
			float SteeringAngle = PhysicsInput->VehicleInputs.Steering * WheelConfig.MaxSteeringAngle;
			SteeringAngle = WheelConfig.InvertSteering ? (SteeringAngle * -1.0f) : SteeringAngle;
			WheelLocalTransform.SetRotation( WheelLocalTransform.TransformRotation(FRotator(0.0f, SteeringAngle, 0.0f).Quaternion()) );
		}

		// We have to calculate the wheel transform every frame because it doesn't have a body in the physics scene
		FTransform WheelWorldTransform = FTransform( VehicleBodyTransform.TransformRotation(WheelLocalTransform.GetRotation()),
			VehicleBodyTransform.TransformPosition(WheelLocalTransform.GetLocation()) );
		FVector WheelWorldLocation = WheelWorldTransform.GetLocation();
		FVector WheelWorldForward = WheelWorldTransform.GetUnitAxis( EAxis::X );
		FVector WheelWorldRight = WheelWorldTransform.GetUnitAxis( EAxis::Y );
		FVector WheelWorldUp = WheelWorldTransform.GetUnitAxis( EAxis::Z );

		FVector TraceStart = WheelWorldLocation + WheelWorldUp * (WheelConfig.SpringLength*0.5f + WheelConfig.WheelRadius); // Top of wheel while compressed
		FVector TraceEnd = WheelWorldLocation - WheelWorldUp * (WheelConfig.SpringLength*0.5f + WheelConfig.WheelRadius); // Bottom of wheel while extended
		
		FHitResult Trace;
		bool TraceHit = UKismetSystemLibrary::LineTraceSingle(this, TraceStart, TraceEnd, UEngineTypes::ConvertToTraceType(WheelConfig.TraceChannel),
															  true, WheelConfig.TraceIgnoreActors, EDrawDebugTrace::None, Trace, true);
		AddDebugTrace(PhysicsOutput, Trace);
		WheelOutput.LastTrace = Trace;
		
		if(TraceHit)
		{
			// Length of spring right now while compressed
			float Length = Trace.Distance - (WheelConfig.WheelRadius * 2.0f);
			float NewSpringLength = TraceHit ? FMath::Clamp(Length, 0.0f, WheelConfig.SpringLength) : WheelConfig.SpringLength;
			WheelOutput.CurrentSpringLength = NewSpringLength; // Used by game thread to place wheel mesh
			
			// Wheel World and Contact Velocity
			const FVector WheelVelocityWorld = UVehicleSystemFunctions::AVS_ChaosGetVelocityAtLocation(PhysicsInput->VehicleMeshPrim, Trace.ImpactPoint);
			const FVector WheelVelocityLocal = WheelWorldTransform.Inverse().TransformVectorNoScale(WheelVelocityWorld);
			//UPrimitiveComponent* ContactComponent = HitResult.GetComponent(); // Get the contact object //TODO :: Chaos Thread equivalent
			const FVector ContactCompVelocityWorld = FVector::ZeroVector;//ContactComponent->GetPhysicsLinearVelocityAtPoint(ImpactPoint);
			const FVector WheelVelocityWorldM = (WheelVelocityWorld - ContactCompVelocityWorld) * 0.01f; // Velocity relative to contacted object (Meters/Second)
			const FVector WheelVelocityProjected = FVector::VectorPlaneProject(WheelVelocityWorldM, Trace.ImpactNormal); // Project speed onto plane
			const FVector WheelVelocityLocalM = WheelWorldTransform.InverseTransformVectorNoScale(WheelVelocityProjected); // Wheel velocity relative to vehicle (Meters/Second)

			// Project the axes onto the plane
			FVector ForwardOnPlane = FVector::VectorPlaneProject(WheelWorldForward, Trace.ImpactNormal); ForwardOnPlane.Normalize();
			FVector RightOnPlane = FVector::VectorPlaneProject(WheelWorldRight, Trace.ImpactNormal); RightOnPlane.Normalize();
			const float WheelVelocity = WheelVelocityProjected.Size(); // Wheel velocity in Meters/Second
			FVector LinearVelocityOnPlaneNormalized; if (WheelVelocity != 0.0f) LinearVelocityOnPlaneNormalized = WheelVelocityProjected / WheelVelocity;

			// Suspension
			const float SpringStrengthNm = WheelConfig.SpringStrength * 1000.0f; // Spring Strength in N/m
			const float ShockAbsorption = WheelConfig.SpringDamping * 1000.0f; // Spring Damper in Ns/m
			const float CompressionDistanceM = (WheelConfig.SpringLength - NewSpringLength) * 0.01f; // Distance of compression in Meters
			const float CompressionVelocityM = WheelVelocityLocal.Z * (-0.01f); // Velocity of compression in Meters/Second

			float SpringForceN = SpringStrengthNm * CompressionDistanceM;
			float DamperForceN = ShockAbsorption * CompressionVelocityM;

			// Suspension :: Excess compression
			if( Length < -1.0f )
			{
				const float VehicleMass = PhysicsInput->VehicleMass; // Mass Kg
				const float Gravity = -World->GetGravityZ();
				const float AntiGravityN = (Gravity * VehicleMass) * 0.01f;
				
				SpringForceN += AntiGravityN;
				DamperForceN *= 2;
			}

			// Scale applied force by wheel's left/right tilt to prevent sudden thrusts when landing sideways
			const float ImpactTilt = 1.0f - FMath::Abs(FVector::DotProduct(Trace.ImpactNormal, WheelWorldRight)); // 1.0f = Wheel is upright, 0.0f = Wheel is sideways (Relative to the Impact Normal)
			constexpr float TiltFalloffStart = 0.5f; // Force starts dropping off here
			constexpr float TiltFalloffEnd = 0.1f; // Zero force here
			const float TiltFalloff = FMath::Clamp((ImpactTilt - TiltFalloffEnd) / (TiltFalloffStart - TiltFalloffEnd), 0.0f, 1.0f);

			const float SuspensionForceN = (SpringForceN + DamperForceN) * TiltFalloff;
			FVector SuspensionForceV = (Trace.ImpactNormal * SuspensionForceN) * 100.0f; // Final suspension force in CentiNewtons

			if( WheelConfig.WheelMode == EWheelMode::Physics )
			{
				// Apply Suspension Forces
				UVehicleSystemFunctions::AVS_ChaosAddForceAtLocation(PhysicsInput->VehicleMeshPrim, Trace.Location, SuspensionForceV);
				UVehicleSystemFunctions::AVS_ChaosAddForce(WheelConfig.WheelPrim, -SuspensionForceV, false);
				AddDebugForce(PhysicsOutput, FDebugForce(Trace.Location, SuspensionForceV, WheelConfig.WheelMode));
				PhysicsOutput.WheelOutputs.Add(WheelOutput); // Add the wheel output since we are ending early

				if( WheelConfig.IsBrakingWheel )
				{
					// Apply Brake Torque
					float BrakeInput = PhysicsInput->VehicleInputs.Brake; // Set BrakeInput as user input if braking wheel
					//BrakeInput = FMath::Clamp((BrakeInput * BrakePressure), WheelConfig.RollingResistance * 0.1f, 1.0f); // Clamp between Resistance & 1, RollingResistance can just be applied as brakes
					if( BrakeInput > 0.0f ) UVehicleSystemFunctions::AVS_ChaosBrakes(WheelConfig.WheelPrim, WheelConfig.BrakeTorque * BrakeInput, ChaosDelta); // TODO: Get physics brake torque to properly accept Nm
					// TODO Physics rolling resistance
				}
				
				continue; // Finish this wheel here, the physics engine handles friction and torque
			}

			// Friction
			const float SurfaceFriction = (Trace.PhysMaterial.IsValid()) ? Trace.PhysMaterial->Friction : 1.0f;
			FVector2D EffectiveFriction = WheelConfig.TireFriction * SurfaceFriction; // Friction combine method = Multiply
			
			// Find current slip angle
			constexpr float RadToDegree = 180 / PI; // convert radians to degrees
			const float ASin = FMath::Asin(FVector::DotProduct(RightOnPlane, LinearVelocityOnPlaneNormalized));
			const float SlipAngle = -ASin * RadToDegree; // Slip angle in degrees

			const float RollingAngVel = WheelVelocityLocal.X / WheelConfig.WheelRadius;
			WheelState.AngularVelocity = RollingAngVel;
			
			// Find SlipX Target
			float XSlipTarget = 0.0f;
			if( (PhysicsInput->VehicleInputs.Handbrake && WheelConfig.IsHandbrakeWheel) || WheelConfig.isLocked ) // Wheel Locking
			{
				WheelState.AngularVelocity = 0.0f;
				XSlipTarget = FMath::Sign(-WheelVelocityLocalM.X);
			}
			else
			{
				const float MaxFrictionTorque = SuspensionForceN * (WheelConfig.WheelRadius * 0.01f) * EffectiveFriction.X; // SpringForce(N) * Radius(M) * Friction

				float BrakeInput = WheelConfig.IsBrakingWheel ? PhysicsInput->VehicleInputs.Brake : 0.0f; // Set BrakeInput as user input if braking wheel
				BrakeInput = FMath::Clamp(BrakeInput, WheelConfig.RollingResistance, 1.0f); // Clamp between Resistance & 1, RollingResistance can just be applied as brakes
				//float XBrakeTorque = (0.0f - RollingAngVel) / ChaosDelta * WheelConfig.Inertia; XBrakeTorque *= BrakeInput;
				float XBrakeTorque = FMath::Sign(WheelState.AngularVelocity * (-1.0f)) * WheelConfig.BrakeTorque * BrakeInput;

				float XDriveTorqueNm = 0.0f;
				if( (PhysicsInput->VehicleInputs.Torque > 0.0f) && WheelConfig.IsDrivingWheel ) // Throttle
				{
					float InputTorque = PhysicsInput->VehicleInputs.Torque;
					if(WheelConfig.InvertTorque ^ PhysicsInput->VehicleInputs.ReverseTorque) InputTorque *= -1.0f; // Invert torque if needed
					float NewAngVel = WheelState.AngularVelocity + ((InputTorque*100.0f) / WheelConfig.Inertia * ChaosDelta);

					// Calculate the XSlip based on the new angular velocity
					XDriveTorqueNm = (NewAngVel - RollingAngVel) / ChaosDelta * WheelConfig.Inertia;
				}

				float XFinalTorque = XBrakeTorque + XDriveTorqueNm; // TODO :: Create debug logger and figure out why this doesn't work
				XSlipTarget = XFinalTorque / MaxFrictionTorque;

				/*if( WIndex == 0 )
				{
					PhysicsOutput.DebugTexts.Add(DEBUG("Physics Thread: XBrakeTorque: %f", XBrakeTorque));
					PhysicsOutput.DebugTexts.Add(DEBUG("Physics Thread: XDriveTorqueNm: %f", XDriveTorqueNm));
					PhysicsOutput.DebugTexts.Add(DEBUG("Physics Thread: XFinalTorque: %f", XFinalTorque));
					PhysicsOutput.DebugTexts.Add(DEBUG("Physics Thread: XSlipTarget: %f", XSlipTarget));
				}*/
			}

			// Interpolate SlipX to target
			float SlipX = WheelState.Slip.X; // Long Slip
			const float MinInterpSpeed = FMath::Clamp(PhysicsInput->VehicleInputs.Throttle * 0.1f, 0.01f, 0.1f);
			const float InterpSpeedLong = FMath::Clamp(FMath::Abs(WheelVelocityLocalM.X) / 0.010f * ChaosDelta, MinInterpSpeed, 1.0f);
			SlipX += (XSlipTarget - SlipX) * InterpSpeedLong;
			SlipX = FMath::Clamp(SlipX, -30.0f, 30.0f); // Long Slip Limit
			
			// Find SlipY Target :: Simply Lerp from LowSpeedSlip -> HighSpeedSlip
			const float YSlipTargetHighSpeed = SlipAngle / 12.0f; // SlipAngle / SlipAnglePeak
			const float YSlipTargetLowSpeed = -FMath::Sign(WheelVelocityLocalM.Y);
			const float Alpha = FMath::GetMappedRangeValueClamped(FVector2D(1.0f, 2.0f), FVector2D(0.0f, 1.0f), WheelVelocity);
			const float YSlipTarget = FMath::Lerp(YSlipTargetLowSpeed, YSlipTargetHighSpeed, Alpha);
			
			// Interpolate SlipY to target
			float SlipY = WheelState.Slip.Y; // Lat Slip
            const float InterpSpeedLat = FMath::Clamp(FMath::Abs(WheelVelocityLocalM.Y) / 0.007f * ChaosDelta, 0.0f, 1.0f);
            SlipY += (YSlipTarget - SlipY) * InterpSpeedLat;
			
			// Create final slip data
            FVector2D Slip = FVector2D(SlipX, SlipY);
			WheelState.Slip = Slip; // Save actual slip data before normalizing for final force
			const float SlipLength = Slip.Size();
            if (SlipLength > 1.0f) // Normalize
            {
                Slip.X /= SlipLength;
                Slip.Y /= SlipLength;
            }
            Slip.Y = FMath::Sign(Slip.Y) * FMath::Sqrt(FMath::Abs(Slip.Y) ); // Square root the Lateral Force
			
			// Traction, the normalized slip defines how much we are using in each direction
            const FVector TractionForward = ForwardOnPlane * Slip.X * EffectiveFriction.X;
            const FVector TractionRight = RightOnPlane * Slip.Y * EffectiveFriction.Y;
            FVector FrictionForceV = ((TractionForward + TractionRight) * SuspensionForceN)*100.0f; // *100.0f to convert to CentiNewtons

			// Apply Forces
			FVector FinalWheelForce = SuspensionForceV + FrictionForceV;
			UVehicleSystemFunctions::AVS_ChaosAddForceAtLocation(PhysicsInput->VehicleMeshPrim, WheelWorldLocation, FinalWheelForce);
			AddDebugForce(PhysicsOutput, FDebugForce(WheelWorldLocation, FinalWheelForce, WheelConfig.WheelMode));
		}
		else // TraceHit
		{
			WheelOutput.CurrentSpringLength = WheelConfig.SpringLength; // Used by game thread to place wheel mesh
			WheelState.Slip = FVector2D::ZeroVector; // No slip while in air

			if( (PhysicsInput->VehicleInputs.Handbrake && WheelConfig.IsHandbrakeWheel) // Handbrake
				|| ((PhysicsInput->VehicleInputs.Brake > 0.0f) && WheelConfig.IsBrakingWheel) ) // Normal brake
			{
				WheelState.AngularVelocity = 0.0f;
			}

			if( WheelConfig.WheelMode == EWheelMode::Physics )
			{
				FTransform PhysWheelTransform = UVehicleSystemFunctions::AVS_GetChaosTransform(WheelConfig.WheelPrim);
				FVector SpringStart = WheelWorldLocation + WheelWorldUp * (WheelConfig.SpringLength * 0.5f);

				float NewSpringLength = FVector::Dist(SpringStart, PhysWheelTransform.GetLocation());
				NewSpringLength = FMath::Clamp(NewSpringLength, 0.0f, WheelConfig.SpringLength);

				if( NewSpringLength < WheelConfig.SpringLength )
				{
					const float SpringStrengthNm = WheelConfig.SpringStrength * 1000.0f; // Spring Strength in N/m
					const float CompressionDistanceM = (WheelConfig.SpringLength - NewSpringLength) * 0.01f; // Distance of compression in Meters
					float SpringForceN = SpringStrengthNm * CompressionDistanceM;
					const float SuspensionForceN = SpringForceN;
					FVector SuspensionForceV = (WheelWorldUp * SuspensionForceN) * 100.0f; // Final suspension force in CentiNewtons

					// Apply Suspension Forces
					UVehicleSystemFunctions::AVS_ChaosAddForceAtLocation(PhysicsInput->VehicleMeshPrim, PhysWheelTransform.GetLocation(), SuspensionForceV);
					UVehicleSystemFunctions::AVS_ChaosAddForce(WheelConfig.WheelPrim, -SuspensionForceV, false);
					AddDebugForce(PhysicsOutput, FDebugForce(PhysWheelTransform.GetLocation(), SuspensionForceV, WheelConfig.WheelMode));
				}
			}
		}
		WheelOutput.AngularVelocity = WheelState.AngularVelocity;
		PhysicsOutput.WheelOutputs.Add(WheelOutput);
	}
}

bool AVehicleSystemBase::SetArrayDisabledCollisions(TArray<UPrimitiveComponent*> Meshes)
{
	using namespace Chaos;
	
	if(IsPhysicsCallbackRegistered())
	{
		TArray<FSingleParticlePhysicsProxy*> ChaosHandles;
		ChaosHandles.Empty();
		for(UPrimitiveComponent* Mesh : Meshes)
		{
			FBodyInstance* BI = Mesh->GetBodyInstance();
			if(!BI)
				return false; // BodyInstance is not valid, failed
				
			FSingleParticlePhysicsProxy* MeshHandle = BI->GetPhysicsActorHandle();
			if(!MeshHandle)
				return false; // Handle is not valid, failed
			
			ChaosHandles.Add(MeshHandle);
			BI->SetContactModification(true);
		}
		for(UPrimitiveComponent* OldMesh : ContactModMeshes)
		{
			// Turn contact mod off on meshes no longer in the array
			if(!Meshes.Contains(OldMesh))
			{
				FBodyInstance* OldBI = OldMesh->GetBodyInstance();
				if(OldBI)
				{
					OldBI->SetContactModification(false);
				}
			}
		}
		ContactModMeshes = Meshes; // Save the new array
		PhysicsThreadCallback->WheelMeshes = ChaosHandles; // Overwrite array, not add, we don't want invalid handles
		return true; // Success
	}
	return false; // Callback is not valid, failed
}
