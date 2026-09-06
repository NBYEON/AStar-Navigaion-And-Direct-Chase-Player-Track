#include "SurvivalHealthComponent.h"

#include "GameFramework/Controller.h"

USurvivalHealthComponent::USurvivalHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USurvivalHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxHealth = FMath::Max(1.0f, MaxHealth);
	CurrentHealth = MaxHealth;
	bDead = false;

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &USurvivalHealthComponent::HandleTakeAnyDamage);
	}
}

float USurvivalHealthComponent::GetHealthNormalized() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}

float USurvivalHealthComponent::Heal(const float Amount, AActor* SourceActor)
{
	if (bDead || Amount <= 0.0f)
	{
		return 0.0f;
	}

	const float PreviousHealth = CurrentHealth;
	SetHealth(CurrentHealth + Amount, SourceActor);
	return CurrentHealth - PreviousHealth;
}

void USurvivalHealthComponent::ResetHealth()
{
	MaxHealth = FMath::Max(1.0f, MaxHealth);
	const float PreviousHealth = CurrentHealth;
	CurrentHealth = MaxHealth;
	bDead = false;
	OnHealthChanged.Broadcast(this, CurrentHealth, MaxHealth, CurrentHealth - PreviousHealth, nullptr);
}

void USurvivalHealthComponent::HandleTakeAnyDamage(
	AActor* DamagedActor,
	const float Damage,
	const UDamageType* DamageType,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	(void)DamagedActor;
	(void)DamageType;

	if (bDead || Damage <= 0.0f)
	{
		return;
	}

	AActor* SourceActor = DamageCauser;
	if (!SourceActor && InstigatedBy)
	{
		SourceActor = InstigatedBy->GetPawn();
	}

	SetHealth(CurrentHealth - Damage, SourceActor);
}

void USurvivalHealthComponent::SetHealth(const float NewHealth, AActor* SourceActor)
{
	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	const float HealthDelta = CurrentHealth - PreviousHealth;
	if (FMath::IsNearlyZero(HealthDelta))
	{
		return;
	}

	OnHealthChanged.Broadcast(this, CurrentHealth, MaxHealth, HealthDelta, SourceActor);
	if (!bDead && CurrentHealth <= 0.0f)
	{
		bDead = true;
		OnDeath.Broadcast(this, SourceActor);
	}
}
