// Copyright 2019-2024 Overtorque Creations LLC. All Rights Reserved. Unauthorized copying of this file, via any medium is strictly prohibited


#include "AVS_DEBUG.h"

DEFINE_LOG_CATEGORY(LogAVS);

void UAVS_DEBUG::LOG(EDebugCategory DebugCategory, const FString& FinalString)
{
	#if AVS_DEBUG
	if( !DebugCategories[DebugCategory].Enabled ) return;
	UE_LOG(LogAVS, Log, TEXT("%s"), *FinalString);
	#endif
}

void UAVS_DEBUG::SCREEN(EDebugCategory DebugCategory, float TimeToDisplay, const FString& FinalString)
{
	#if AVS_DEBUG
	const FDebugCategoryData& DebugCategoryData = DebugCategories[DebugCategory];
	if( !DebugCategoryData.Enabled ) return;
	if( GEngine ) GEngine->AddOnScreenDebugMessage(-1, TimeToDisplay, DebugCategoryData.Color, FinalString);
	#endif
}

void UAVS_DEBUG::SCREEN(EDebugCategory DebugCategory, const FString& FinalString)
{
	#if AVS_DEBUG
	SCREEN(DebugCategory, 5.0f, FinalString);
	#endif
}

void UAVS_DEBUG::SCREENLOG(EDebugCategory DebugCategory, float TimeToDisplay, const FString& FinalString)
{
	#if AVS_DEBUG
	SCREEN(DebugCategory, TimeToDisplay, FinalString);
	LOG(DebugCategory, FinalString);
	#endif
}
