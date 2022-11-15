#pragma once
UENUM(BlueprintType)
enum class ECombatState : uint8
{
	ECS_Free UMETA(DisplayName = "Free"),
	ECS_Attack UMETA(DisplayName = "Attack"),
	ECS_Defend UMETA(DisplayName = "Defend"),
	ECS_Hitted UMETA(DisplayName = "Hitted"),
	ECS_Dead UMETA(DisplayName = "Dead"),
	ECS_MAX UMETA(DisplayName = "MAX")
};