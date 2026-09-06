#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZombiePathfindingComponent.generated.h"

class ACharacter;

/** A small, static, four-neighbour A* grid intended for prototype-scale enemy navigation. */
UCLASS(ClassGroup = (AI), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class MYPROJECT_API UZombiePathfindingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZombiePathfindingComponent();

	/** World-space south-west corner of the grid, placed at floor level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Navigation")
	FVector GridOrigin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Navigation", meta = (ClampMin = "1", UIMin = "1"))
	int32 GridWidth = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Navigation", meta = (ClampMin = "1", UIMin = "1"))
	int32 GridHeight = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Navigation", meta = (ClampMin = "10.0", UIMin = "10.0"))
	float CellSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Navigation", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AgentRadius = 42.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Navigation", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AgentHalfHeight = 88.0f;

	/** Maximum grid-cell radius used to recover a zombie that starts in a blocked clearance cell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Navigation", meta = (ClampMin = "1", UIMin = "1", ClampMax = "10", UIMax = "10"))
	int32 StartRecoveryRadiusCells = 4;

	/** Collision channel used to find ground and static obstacles while creating the grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Navigation")
	TEnumAsByte<ECollisionChannel> ObstacleChannel = ECC_WorldStatic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie Navigation|Debug")
	bool bDrawGridWhenBuilt = false;

	/** Rebuilds the static collision grid. Call after the component has a world. */
	UFUNCTION(BlueprintCallable, Category = "Zombie Navigation")
	bool BuildGrid();

	/**
	 * Finds a four-neighbour A* path. Returned points normally exclude the start cell.
	 * If the start is blocked, a reachable green recovery cell is included as the first waypoint.
	 * OutFailureReason is empty when the request succeeds and explains a rejected request otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Zombie Navigation")
	bool FindPath(const FVector& StartLocation, const FVector& GoalLocation, TArray<FVector>& OutWaypoints, FString& OutFailureReason);

	UFUNCTION(BlueprintPure, Category = "Zombie Navigation")
	bool IsGridBuilt() const { return bGridBuilt; }

	/** Conservative direct-walk test for level ground, independent of grid bounds.
	 * Uses the moving character's capsule and walkable slope. Samples support along
	 * the route; stairs/uneven ground fall back to A*. Call on the brain timer.
	 */
	UFUNCTION(BlueprintCallable, Category = "Zombie Navigation")
	bool CanWalkDirectly(ACharacter* MovingCharacter, FVector GoalLocation, FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Zombie Navigation|Debug")
	void DrawPath(const TArray<FVector>& PathPoints, float Duration = 2.0f);

private:
	struct FGridNode
	{
		FVector Center = FVector::ZeroVector;
		bool bWalkable = false;
	};

	TArray<FGridNode> GridNodes;
	bool bGridBuilt = false;

	int32 ToIndex(int32 X, int32 Y) const;
	bool IsValidCell(int32 X, int32 Y) const;
	bool WorldToCell(const FVector& WorldLocation, int32& OutX, int32& OutY) const;
	FVector CellToWorld(int32 X, int32 Y) const;
	int32 Heuristic(int32 FromIndex, int32 ToIndex) const;
	bool IsCellWalkable(int32 X, int32 Y, FVector& OutCenter) const;
	bool IsRecoveryCellReachable(const FVector& StartLocation, const FVector& CellCenter) const;
	bool FindRecoveryStartCell(const FVector& StartLocation, int32 BlockedX, int32 BlockedY, int32& OutX, int32& OutY) const;
};
