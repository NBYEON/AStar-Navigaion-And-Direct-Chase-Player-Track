#include "ZombiePathfindingComponent.h"

#include "Algo/Reverse.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

UZombiePathfindingComponent::UZombiePathfindingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UZombiePathfindingComponent::ToIndex(const int32 X, const int32 Y) const
{
	return X + Y * GridWidth;
}

bool UZombiePathfindingComponent::IsValidCell(const int32 X, const int32 Y) const
{
	return X >= 0 && X < GridWidth && Y >= 0 && Y < GridHeight;
}

FVector UZombiePathfindingComponent::CellToWorld(const int32 X, const int32 Y) const
{
	return GridOrigin + FVector((X + 0.5f) * CellSize, (Y + 0.5f) * CellSize, 0.0f);
}

bool UZombiePathfindingComponent::WorldToCell(const FVector& WorldLocation, int32& OutX, int32& OutY) const
{
	OutX = FMath::FloorToInt((WorldLocation.X - GridOrigin.X) / CellSize);
	OutY = FMath::FloorToInt((WorldLocation.Y - GridOrigin.Y) / CellSize);
	return IsValidCell(OutX, OutY);
}

int32 UZombiePathfindingComponent::Heuristic(const int32 FromIndex, const int32 ToIndex) const
{
	const int32 FromX = FromIndex % GridWidth;
	const int32 FromY = FromIndex / GridWidth;
	const int32 ToX = ToIndex % GridWidth;
	const int32 ToY = ToIndex / GridWidth;
	return (FMath::Abs(FromX - ToX) + FMath::Abs(FromY - ToY)) * 10;
}

bool UZombiePathfindingComponent::IsCellWalkable(const int32 X, const int32 Y, FVector& OutCenter) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector CellFloorLocation = CellToWorld(X, Y);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZombieAStarGrid), false, GetOwner());
	// Preserve the configured trace channel used by the original grid, but ignore
	// every Pawn so player/zombie capsules never become static navigation obstacles.
	for (TActorIterator<APawn> PawnIterator(World); PawnIterator; ++PawnIterator)
	{
		QueryParams.AddIgnoredActor(*PawnIterator);
	}

	FHitResult FloorHit;
	const FVector FloorTraceStart = CellFloorLocation + FVector(0.0f, 0.0f, 200.0f);
	const FVector FloorTraceEnd = CellFloorLocation - FVector(0.0f, 0.0f, 200.0f);
	if (!World->LineTraceSingleByChannel(FloorHit, FloorTraceStart, FloorTraceEnd, ObstacleChannel, QueryParams))
	{
		return false;
	}

	OutCenter = FloorHit.ImpactPoint + FVector(0.0f, 0.0f, AgentHalfHeight + 2.0f);
	const FCollisionShape AgentShape = FCollisionShape::MakeCapsule(AgentRadius, AgentHalfHeight);
	return !World->OverlapBlockingTestByChannel(OutCenter, FQuat::Identity, ObstacleChannel, AgentShape, QueryParams);
}

bool UZombiePathfindingComponent::BuildGrid()
{
	if (GridWidth <= 0 || GridHeight <= 0 || CellSize <= 0.0f || !GetWorld())
	{
		bGridBuilt = false;
		return false;
	}

	GridNodes.Reset();
	GridNodes.SetNum(GridWidth * GridHeight);

	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			FGridNode& Node = GridNodes[ToIndex(X, Y)];
			Node.bWalkable = IsCellWalkable(X, Y, Node.Center);

			if (bDrawGridWhenBuilt)
			{
				const FColor CellColor = Node.bWalkable ? FColor::Green : FColor::Red;
				DrawDebugBox(GetWorld(), Node.Center, FVector(CellSize * 0.45f, CellSize * 0.45f, 5.0f), CellColor, false, 10.0f, 0, 2.0f);
			}
		}
	}

	bGridBuilt = true;
	return true;
}

bool UZombiePathfindingComponent::IsRecoveryCellReachable(const FVector& StartLocation, const FVector& CellCenter) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZombieAStarStartRecovery), false, GetOwner());
	for (TActorIterator<APawn> PawnIterator(World); PawnIterator; ++PawnIterator)
	{
		QueryParams.AddIgnoredActor(*PawnIterator);
	}

	const FVector Lift(0.0f, 0.0f, 3.0f);
	const FCollisionShape AgentShape = FCollisionShape::MakeCapsule(AgentRadius, AgentHalfHeight);
	FHitResult Hit;
	return !World->SweepSingleByChannel(
		Hit,
		StartLocation + Lift,
		CellCenter + Lift,
		FQuat::Identity,
		ObstacleChannel,
		AgentShape,
		QueryParams);
}

bool UZombiePathfindingComponent::FindRecoveryStartCell(
	const FVector& StartLocation,
	const int32 BlockedX,
	const int32 BlockedY,
	int32& OutX,
	int32& OutY) const
{
	for (int32 Radius = 1; Radius <= StartRecoveryRadiusCells; ++Radius)
	{
		bool bFoundAtRadius = false;
		double BestDistanceSquared = TNumericLimits<double>::Max();
		int32 BestX = INDEX_NONE;
		int32 BestY = INDEX_NONE;

		for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
		{
			for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
			{
				if (FMath::Max(FMath::Abs(OffsetX), FMath::Abs(OffsetY)) != Radius)
				{
					continue;
				}

				const int32 CandidateX = BlockedX + OffsetX;
				const int32 CandidateY = BlockedY + OffsetY;
				if (!IsValidCell(CandidateX, CandidateY))
				{
					continue;
				}

				const FGridNode& Candidate = GridNodes[ToIndex(CandidateX, CandidateY)];
				if (!Candidate.bWalkable || !IsRecoveryCellReachable(StartLocation, Candidate.Center))
				{
					continue;
				}

				const double DistanceSquared = FVector::DistSquared2D(StartLocation, Candidate.Center);
				if (!bFoundAtRadius || DistanceSquared < BestDistanceSquared)
				{
					bFoundAtRadius = true;
					BestDistanceSquared = DistanceSquared;
					BestX = CandidateX;
					BestY = CandidateY;
				}
			}
		}

		if (bFoundAtRadius)
		{
			OutX = BestX;
			OutY = BestY;
			return true;
		}
	}

	return false;
}

bool UZombiePathfindingComponent::FindPath(const FVector& StartLocation, const FVector& GoalLocation, TArray<FVector>& OutWaypoints, FString& OutFailureReason)
{
	OutWaypoints.Reset();
	OutFailureReason.Reset();

	auto FailPath = [&OutFailureReason](const TCHAR* Reason)
	{
		OutFailureReason = Reason;
		return false;
	};

	if (!bGridBuilt)
	{
		return FailPath(TEXT("Grid has not been built yet."));
	}

	int32 StartX;
	int32 StartY;
	
	int32 GoalX;
	int32 GoalY;
	const bool bStartInsideGrid = WorldToCell(StartLocation, StartX, StartY);
	const bool bGoalInsideGrid = WorldToCell(GoalLocation, GoalX, GoalY);
	if (!bStartInsideGrid)
	{
		return FailPath(TEXT("Zombie start location is outside this navigation grid."));
	}
	if (!bGoalInsideGrid)
	{
		return FailPath(TEXT("Goal location is outside this navigation grid."));
	}

	bool bRecoveredStart = false;
	int32 StartIndex = ToIndex(StartX, StartY);
	const int32 GoalIndex = ToIndex(GoalX, GoalY);
	if (!GridNodes[StartIndex].bWalkable)
	{
		if (!FindRecoveryStartCell(StartLocation, StartX, StartY, StartX, StartY))
		{
			return FailPath(TEXT("Zombie start cell is blocked and no nearby reachable green cell was found."));
		}
		StartIndex = ToIndex(StartX, StartY);
		bRecoveredStart = true;
	}
	if (!GridNodes[GoalIndex].bWalkable)
	{
		return FailPath(TEXT("Goal cell is blocked."));
	}

	const int32 NodeCount = GridNodes.Num();
	TArray<int32> GCost;
	TArray<int32> Parent;
	TArray<bool> bClosed;
	TArray<int32> OpenSet;
	GCost.Init(MAX_int32, NodeCount);
	Parent.Init(INDEX_NONE, NodeCount);
	bClosed.Init(false, NodeCount);

	GCost[StartIndex] = 0;
	OpenSet.Add(StartIndex);

	while (!OpenSet.IsEmpty())
	{
		int32 BestOpenPosition = 0;
		int32 CurrentIndex = OpenSet[0];
		int32 BestScore = GCost[CurrentIndex] + Heuristic(CurrentIndex, GoalIndex);

		for (int32 OpenPosition = 1; OpenPosition < OpenSet.Num(); ++OpenPosition)
		{
			const int32 CandidateIndex = OpenSet[OpenPosition];
			const int32 CandidateScore = GCost[CandidateIndex] + Heuristic(CandidateIndex, GoalIndex);
			if (CandidateScore < BestScore || (CandidateScore == BestScore && Heuristic(CandidateIndex, GoalIndex) < Heuristic(CurrentIndex, GoalIndex)))
			{
				BestOpenPosition = OpenPosition;
				CurrentIndex = CandidateIndex;
				BestScore = CandidateScore;
			}
		}

		OpenSet.RemoveAtSwap(BestOpenPosition);
		if (CurrentIndex == GoalIndex)
		{
			TArray<FVector> ReversePath;
			for (int32 PathIndex = GoalIndex; PathIndex != INDEX_NONE; PathIndex = Parent[PathIndex])
			{
				ReversePath.Add(GridNodes[PathIndex].Center);
			}

			Algo::Reverse(ReversePath);
			if (!bRecoveredStart && !ReversePath.IsEmpty())
			{
				ReversePath.RemoveAt(0);
			}
			OutWaypoints = MoveTemp(ReversePath);
			return true;
		}

		bClosed[CurrentIndex] = true;
		const int32 CurrentX = CurrentIndex % GridWidth;
		const int32 CurrentY = CurrentIndex / GridWidth;
		constexpr int32 NeighborOffsets[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

		for (const int32 (&Offset)[2] : NeighborOffsets)
		{
			const int32 NeighborX = CurrentX + Offset[0];
			const int32 NeighborY = CurrentY + Offset[1];
			if (!IsValidCell(NeighborX, NeighborY))
			{
				continue;
			}

			const int32 NeighborIndex = ToIndex(NeighborX, NeighborY);
			if (bClosed[NeighborIndex] || !GridNodes[NeighborIndex].bWalkable)
			{
				continue;
			}

			const int32 TentativeGCost = GCost[CurrentIndex] + 10;
			if (TentativeGCost < GCost[NeighborIndex])
			{
				GCost[NeighborIndex] = TentativeGCost;
				Parent[NeighborIndex] = CurrentIndex;
				OpenSet.AddUnique(NeighborIndex);
			}
		}
	}

	return FailPath(TEXT("No connected walkable route exists between start and goal."));
}

bool UZombiePathfindingComponent::CanWalkDirectly(ACharacter* MovingCharacter, FVector GoalLocation, FString& OutFailureReason)
{
	OutFailureReason.Reset();
	auto Fail = [&OutFailureReason](const TCHAR* Reason)
	{
		OutFailureReason = Reason;
		return false;
	};
	if (!GetWorld() || !IsValid(MovingCharacter) || GoalLocation.ContainsNaN())
		return Fail(TEXT("Invalid character, world or goal."));
	const UCapsuleComponent* Capsule = MovingCharacter->GetCapsuleComponent();
	const UCharacterMovementComponent* Movement = MovingCharacter->GetCharacterMovement();
	if (!Capsule || !Movement || !Movement->IsMovingOnGround())
		return Fail(TEXT("Character must be walking on ground."));

	const FVector Start = Capsule->GetComponentLocation();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	if (FMath::Abs(GoalLocation.Z - Start.Z) > 100.0f)
		return Fail(TEXT("Goal is on a different height; use A*."));
	GoalLocation.Z = Start.Z;
	const double Distance = FVector::Dist2D(Start, GoalLocation);
	const double Spacing = FMath::Max(10.0f, Radius * 0.5f);
	if (Distance > 3000.0 || Distance / Spacing > 256.0)
		return Fail(TEXT("Direct route exceeds the bounded probe range."));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieDirectRoute), false, MovingCharacter);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		Params.AddIgnoredActor(*It);
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	const FVector Lift(0, 0, 2.0f);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FHitResult Hit;
	if (GetWorld()->OverlapBlockingTestByChannel(Start + Lift, FQuat::Identity, Channel, Shape, Params, Responses)
		|| GetWorld()->SweepSingleByChannel(Hit, Start + Lift, GoalLocation + Lift, FQuat::Identity, Channel, Shape, Params, Responses))
		return Fail(TEXT("Capsule route is obstructed."));

	const int32 Steps = FMath::Max(1, FMath::CeilToInt(Distance / Spacing));
	const FVector Side = FVector::CrossProduct((GoalLocation - Start).GetSafeNormal2D(), FVector::UpVector) * Radius * 0.7f;
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		const FVector Feet = FMath::Lerp(Start, GoalLocation, static_cast<double>(Step) / Steps) - FVector(0, 0, HalfHeight);
		// Three support samples across the footprint. Small gaps below sample spacing
		// are not guaranteed to be detected; this is a conservative prototype probe.
		for (int32 Lane = -1; Lane <= 1; ++Lane)
		{
			const FVector Sample = Feet + Side * Lane;
			if (!GetWorld()->LineTraceSingleByChannel(Hit, Sample + FVector(0, 0, 10), Sample - FVector(0, 0, 10), Channel, Params, Responses)
				|| !Movement->IsWalkable(Hit))
				return Fail(TEXT("Missing or unwalkable ground along direct route."));
		}
	}
	return true;
}

void UZombiePathfindingComponent::DrawPath(const TArray<FVector>& PathPoints, const float Duration)
{
	UWorld* World = GetWorld();
	if (!World || PathPoints.Num() < 2)
	{
		return;
	}

	for (int32 PointIndex = 1; PointIndex < PathPoints.Num(); ++PointIndex)
	{
		DrawDebugLine(World, PathPoints[PointIndex - 1] + FVector(0.0f, 0.0f, 20.0f), PathPoints[PointIndex] + FVector(0.0f, 0.0f, 20.0f), FColor::Cyan, false, Duration, 0, 4.0f);
	}
}
