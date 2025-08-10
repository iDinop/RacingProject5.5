// Copyright 2019-2024 Overtorque Creations LLC. All Rights Reserved.
// Unauthorized copying of this file, via any medium is strictly prohibited

#pragma once

#include "Components/SceneComponent.h"
#include "VehicleComponent.generated.h"

UCLASS(Abstract, Blueprintable, ClassGroup="VehicleSystem", meta=(BlueprintSpawnableComponent))
class VEHICLESYSTEMPLUGIN_API UVehicleComponent : public USceneComponent
{
	GENERATED_BODY()
};
