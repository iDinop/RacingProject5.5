// Copyright 2019-2024 Overtorque Creations LLC. All Rights Reserved. Unauthorized copying of this file, via any medium is strictly prohibited

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AVS_DEBUG.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAVS, Log, All);

// Global debug toggle
#define AVS_DEBUG_ENABLED false

// macro for wrapping code, prevents debug in shipping
#define AVS_DEBUG UE_BUILD_SHIPPING ? false : AVS_DEBUG_ENABLED

#define TXT(Format, ...) FString::Printf(TEXT(Format), __VA_ARGS__)

enum class EDebugCategory
{
	NETWORK,
	PHYSICS,
};

struct FDebugCategoryData
{
	bool Enabled;
	FColor Color;

	FDebugCategoryData(bool InEnabled, FColor InColor) : Enabled(InEnabled), Color(InColor) {}
};

UCLASS()
class VEHICLESYSTEMPLUGIN_API UAVS_DEBUG : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

private:
	inline static const TMap<EDebugCategory, FDebugCategoryData> DebugCategories =
	{
		{EDebugCategory::NETWORK, {false, FColor::Green}},
		{EDebugCategory::PHYSICS, {false, FColor::Orange}}
	};

public:
	static void LOG(EDebugCategory DebugCategory, const FString& FinalString);

	static void SCREEN(EDebugCategory DebugCategory, float TimeToDisplay, const FString& FinalString);
	static void SCREEN(EDebugCategory DebugCategory, const FString& FinalString);

	static void SCREENLOG(EDebugCategory DebugCategory, float TimeToDisplay, const FString& FinalString);
};
