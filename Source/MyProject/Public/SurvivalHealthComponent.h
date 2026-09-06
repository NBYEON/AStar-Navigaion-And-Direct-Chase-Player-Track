#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SurvivalHealthComponent.generated.h"

class AController;
class UDamageType;
class USurvivalHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOnSurvivalHealthChanged,
	USurvivalHealthComponent*, HealthComponent,
	float, CurrentHealth,
	float, MaxHealth,
	float, HealthDelta,
	AActor*, SourceActor);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnSurvivalDeath,
	USurvivalHealthComponent*, HealthComponent,
	AActor*, KillerActor);

/** Reusable health, damage and death state for zombies and other damageable actors. */
UCLASS(ClassGroup = (Gameplay), BlueprintType, meta = (BlueprintSpawnableComponent))
class MYPROJECT_API USurvivalHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USurvivalHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Health", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Survival|Health")
	float CurrentHealth = 0.0f;

	UPROPERTY(BlueprintAssignable, Category = "Survival|Health")
	FOnSurvivalHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Survival|Health")
	FOnSurvivalDeath OnDeath;

	UFUNCTION(BlueprintPure, Category = "Survival|Health")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "Survival|Health")
	float GetHealthNormalized() const;

	/** Restores health without exceeding MaxHealth. Returns the amount actually restored. */
	UFUNCTION(BlueprintCallable, Category = "Survival|Health")
	float Heal(float Amount, AActor* SourceActor);

	/** Restores this component to MaxHealth, suitable for a newly reused actor. */
	UFUNCTION(BlueprintCallable, Category = "Survival|Health")
	void ResetHealth();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Survival|Health")
	bool bDead = false;

	UFUNCTION()
	void HandleTakeAnyDamage(
		AActor* DamagedActor,
		float Damage,
		const UDamageType* DamageType,
		AController* InstigatedBy,
		AActor* DamageCauser);

	void SetHealth(float NewHealth, AActor* SourceActor);
};
