// Copyright 2019-2024 Overtorque Creations LLC. All Rights Reserved.
// Unauthorized copying of this file, via any medium is strictly prohibited

#pragma once

#include "CoreMinimal.h"
#include "VehicleWheelBase.h"
#include "VehiclePhysicsCallback.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Runtime/Engine/Classes/Curves/CurveFloat.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "VehicleSystemBase.generated.h"

USTRUCT(BlueprintType)
struct FNetState
{
	GENERATED_BODY()

	UPROPERTY()
	float NetTimestamp;
	UPROPERTY(NotReplicated)
	float LocalTimestamp;
	UPROPERTY()
	FVector position;
	UPROPERTY()
	FRotator rotation;
	UPROPERTY()
	FVector velocity;
	UPROPERTY()
	FVector angularVelocity;

	FNetState()
	{
		NetTimestamp = 0.0f;
		LocalTimestamp = 0.0f;
		position = FVector::ZeroVector;
		rotation = FRotator::ZeroRotator;
		velocity = FVector::ZeroVector;
		angularVelocity = FVector::ZeroVector;
	}
};

UENUM(BlueprintType)
enum class NetworkRoles : uint8
{
	None, Owner, Server, Client, ClientSpawned
};

UENUM(BlueprintType)
enum class SteeringSmoothingType : uint8
{
	Instant, Constant, Ease
};

USTRUCT(BlueprintType)
struct FVehicleGear
{
	GENERATED_BODY()

	/** Maximum speed of the gear */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Transmission")
	float EndSpeed = 0.0f;

	/** Speed at which this gear will be at its maximum torque */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Transmission")
	float StartSpeed = 0.0f;

	/** Automatic Transmission Only, Transmission chooses a new gear when above this speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Transmission")
	float UpShift = 0.0f;

	/** Automatic Transmission Only, Transmission chooses a new gear when below this speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Transmission")
	float DownShift = 0.0f;

	/** RPM at the EndSpeed of the gear */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Transmission")
	float HighRPM = 0.0f;

	/** RPM at the StartSpeed of the gear */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Transmission")
	float LowRPM = 0.0f;

	/** Torque at the StartSpeed of the gear */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Transmission")
	float MaxTorque = 0.0f;

	/** Torque at the EndSpeed of the gear */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Transmission")
	float MinTorque = 0.0f;
};

UCLASS(Blueprintable, Abstract)
class VEHICLESYSTEMPLUGIN_API AVehicleSystemBase : public APawn
{
	GENERATED_BODY()

private:
	UPROPERTY()
	TArray<UVehicleWheelBase*> VehicleWheels;

	// ** Physics Thread ** //

	FVehiclePhysicsCallback* PhysicsThreadCallback;

	UPROPERTY()
	TArray<UPrimitiveComponent*> ContactModMeshes;

	TArray<FAVS1_Wheel_State> WheelStates;

protected: // Accessible by subclasses

	// ** Overrides ** //

	//virtual void OnConstruction(const FTransform& Transform) override; // Construction Script
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ** Tick ** //

	float TickDeltaTime = 0.0f;
	void AlwaysTick();
	void PassiveTick(float DeltaTime);
	void NetworkTick();

	// Internal AVS function! Tick used for AVS functions
	UFUNCTION(BlueprintImplementableEvent, Category = "VehicleSystemPlugin")
	void AVS_Tick(float DeltaTime);

	// Called while the normal Tick function is disabled due to gatekeeping
	UFUNCTION(BlueprintImplementableEvent, Category = "Vehicle - General", meta=(DisplayName = "AVS_PassiveTick"))
	void PassiveTickBP(float DeltaTime);

	// ** Passive / Rest ** //

	// Called whenever the passive state changes modes
	UFUNCTION(BlueprintNativeEvent, Category = "VehicleSystemPlugin")
	void PassiveStateChanged(bool NewPassiveState);

	// Allows blueprint to determine the current passive state
	UFUNCTION(BlueprintImplementableEvent, Category = "VehicleSystemPlugin")
	bool DeterminePassiveState();

	void DetermineLocalRestState();

	// ** Networking ** //
	#pragma region Networking

	float GetLocalWorldTime()
	{
		return GetWorld()->GetTimeSeconds();
	}

	float GetNetworkWorldTime()
	{
		if( !IsValid(GetWorld()->GetGameState()) ) return 0.0f; // Game state is not always valid on clients
		return GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
	}

	bool ShouldSyncWithServer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Network")
	bool ReplicateMovement;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Network")
	bool SyncLocation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Network")
	bool SyncRotation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Network", AdvancedDisplay)
	float NetSendRate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Network", AdvancedDisplay)
	float NetTimeBehind;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Network", AdvancedDisplay)
	float NetLerpStart;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Network", AdvancedDisplay)
	float NetPositionTolerance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Network", AdvancedDisplay)
	float NetSmoothing;

	UPROPERTY(ReplicatedUsing=OnRep_RestState)
	FNetState RestState;

	bool RestThresh = false;

	UFUNCTION()
	void OnRep_RestState()
	{
		NetworkAtRest = (RestState.position != FVector::ZeroVector);
	}

	// Vehicle is considered at rest for network purposes
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle - Network")
	bool NetworkAtRest = false;
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle - Physics")
	float RestTimer = 0.0f;

	//UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "VehicleSystemPlugin")
	void SetVehicleLocation(const FVector& InNewPos, const FRotator& InNewRot, bool WakeWheels = false);

	UFUNCTION(BlueprintImplementableEvent, Category = "VehicleSystemPlugin")
	void TeleportWheels();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VehicleSystemPlugin")
	void WakeWheelsForMovement();

	TArray<FNetState> StateQueue;
	FNetState LerpStartState;
	bool CreateNewStartState = true;
	float LastActiveTimestamp = 0;

	FTimerHandle NetSendTimer;
	UFUNCTION()
	void NetStateSend();

	/**Used to temporarily disable movement replication, does not change ReplicateMovement */
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin")
	void SetShouldSyncWithServer(bool ShouldSync);

	void SetReplicationTimer(bool Enabled);
	FNetState CreateNetStateForNow();
	void AddStateToQueue(FNetState StateToAdd);
	void ClearQueue();
	void CalculateTimestamps();
	void SyncPhysics();
	void LerpToNetState(FNetState NextState, float CurrentServerTime);
	void ApplyExactNetState(FNetState State);

	bool isServer()
	{
		UWorld* World = GetWorld();
		return World ? (World->GetNetMode() != NM_Client) : false;
	}

	NetworkRoles GetNetworkRole()
	{
		if( IsLocallyControlled() )
		{
			//I'm controlling this
			return NetworkRoles::Owner;
		}
		else if( isServer() )
		{
			if( IsPlayerControlled() )
			{
				//I'm the server, and a client is controlling this
				return NetworkRoles::Server;
			}
			else
			{
				//I'm the server, and I'm controlling this because it's unpossessed
				return NetworkRoles::Owner;
			}
		}
		else if( GetLocalRole() == ROLE_Authority )
		{
			//I'm not the server, I'm not controlling this, and I have authority.
			return NetworkRoles::ClientSpawned;
		}
		else
		{
			//I'm a client and I'm not controlling this
			return NetworkRoles::Client;
		}
		//return NetworkRoles::None;
	}

	UFUNCTION(Server, unreliable, WithValidation)
	void Server_ReceiveNetState(FNetState State);
	virtual bool Server_ReceiveNetState_Validate(FNetState State);
	virtual void Server_ReceiveNetState_Implementation(FNetState State);
	UFUNCTION(NetMulticast, unreliable, WithValidation)
	void Client_ReceiveNetState(FNetState State);
	virtual bool Client_ReceiveNetState_Validate(FNetState State);
	virtual void Client_ReceiveNetState_Implementation(FNetState State);
	UFUNCTION(Server, reliable, WithValidation)
	void Server_ReceiveRestState(FNetState State);
	virtual bool Server_ReceiveRestState_Validate(FNetState State);
	virtual void Server_ReceiveRestState_Implementation(FNetState State);
	UFUNCTION(NetMulticast, reliable, WithValidation)
	void Multicast_ChangedOwner();
	virtual bool Multicast_ChangedOwner_Validate();
	virtual void Multicast_ChangedOwner_Implementation();

	/** Called when the owning client changes (Possessed or UnPossessed) */
	UFUNCTION(BlueprintImplementableEvent, Category = "VehicleSystemPlugin")
	void OwnerChanged();
	#pragma endregion Networking

	// ** Physics Thread ** //

	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin")
	void UpdateInternalWheelArray();

	// Register async callback with physics system.
	bool IsPhysicsCallbackRegistered();
	void RegisterPhysicsCallback();

	// AVS internal use only! Sets array of meshes with collisions disabled against the VehicleMesh
	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin")
	bool SetArrayDisabledCollisions(TArray<UPrimitiveComponent*> Meshes);

	/** Tick delta of the chaos physics thread (most recent output). Don't use in calculations, for display purposes only*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle - Physics")
	float ChaosDeltaTime = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "VehicleSystemPlugin")
	FAVS_Inputs InputsForPhysicsThread;

	UFUNCTION(BlueprintCallable, Category = "VehicleSystemPlugin")
	void PhysicsThreadInputs(FAVS_Inputs NewInputs)
	{
		InputsForPhysicsThread = NewInputs;
	}

	// ** Debug ** //

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VehicleSystemPlugin")
	TArray<FHitResult> DebugTraces;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VehicleSystemPlugin")
	TArray<FDebugForce> DebugForces;

	static void AddDebugTrace(FVehiclePhysicsPhysicsOutput& PhysicsOutput, const FHitResult& Trace)
	{
		#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		PhysicsOutput.DebugTraces.Add(Trace);
		#endif
	}

	static void AddDebugForce(FVehiclePhysicsPhysicsOutput& PhysicsOutput, const FDebugForce& Force)
	{
		#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		PhysicsOutput.DebugForces.Add(Force);
		#endif
	}

	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintDebugMessage(const FString& text);

	/*UFUNCTION(BlueprintImplementableEvent, Category = "VehicleSystemPlugin")
		void DebugReceivedNetState(float timestamp, float serverDelta, FVector position, FRotator rotation); // Used for development */

public:
	AVehicleSystemBase();

	// ** Overrides ** //

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;
	virtual void Tick(float DeltaTime) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle - General", meta=(AllowPrivateAccess = "true"))
	UStaticMeshComponent* VehicleMesh;

	// ** Physics Thread ** //

	void AVS_PhysicsTick(float ChaosDelta, const FVehiclePhysicsPhysicsInput* PhysicsInput, FVehiclePhysicsPhysicsOutput& PhysicsOutput);

	// ** Passive / Rest ** //

	// Low resource mode, should be active when completely idle
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle - General")
	bool PassiveMode = false;

	// Should passive mode gatekeep the standard Tick event, highly recommended to keep this true.
	// You can use the "AVS_PassiveTick" function for things that need to always tick
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - General", AdvancedDisplay)
	bool PassiveTickGatekeeping = true;

	// Velocity (cm) at which the vehicle is considered moving, used for network rest state and passive mode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Physics", AdvancedDisplay)
	float RestVelocityThreshold = 25.0f;

	// Vehicle is at rest locally (not tied to networking)
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle - General")
	bool LocalVehicleAtRest = false;

	// ** Config ** //

	/** Max steering input based on the vehicle speed */
	UPROPERTY(EditAnywhere, Category = "Vehicle - General", meta=(XAxisName="Speed", YAxisName="Steering" ))
	FRuntimeFloatCurve SteeringFalloffCurve;

	// Type of smoothing to apply to steering input
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - General")
	SteeringSmoothingType SteeringInputSmoothing = SteeringSmoothingType::Ease;

	// Higher value = faster
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - General", meta=(EditCondition="SteeringInputSmoothing != SteeringSmoothingType::Instant"))
	float SteeringSpeed = 2.5f;

	// Special SteeringSpeed for when steering is being recentered, generally from a zero input
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - General", meta=(EditCondition="SteeringInputSmoothing != SteeringSmoothingType::Instant"))
	float SteeringRecenterSpeed = 2.5f;

	UFUNCTION(BlueprintPure, Category = "VehicleSystemPlugin")
	float GetMaxSteeringInput(float Speed) { return FMath::Clamp(SteeringFalloffCurve.GetRichCurveConst()->Eval(Speed), 0.0f, 1.0f); };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle - Transmission")
	TArray<FVehicleGear> Gears;

	UFUNCTION(BlueprintPure, Category = "VehicleSystemPlugin")
	float GetSteeringSpeed(float OldSteering, float NewSteering)
	{
		if( IsTowardZero(OldSteering, NewSteering) ) { return SteeringRecenterSpeed; }
		return SteeringSpeed;
	};

	// Returns true if the newer value is moving towards zero ( AKA -0.8 to -0.2 = true )
	UFUNCTION(BlueprintPure, Category = "VehicleSystemPlugin", meta=(Keywords="Torward Torwards"))
	static bool IsTowardZero(float Old, float New) { return FMath::Abs(Old) > FMath::Abs(New); };

	static float GetPercentBetweenValues(float Value, float Begin, float End)
	{
		return (Value - Begin) / (End - Begin);
	}
};
