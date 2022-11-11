

#include "CombatCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Combat/MyComponents/CombatComponent.h"
#include "Combat/MyComponents/CollisionComponent.h"
#include "Combat/MyComponents/StatsComponent.h"
#include "Combat/MyComponents/FocusComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
//
#include "Combat/PlayerController/CombatPlayerController.h"

#include "Components/CapsuleComponent.h"


ACombatCharacter::ACombatCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	// Camera boom
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->bUsePawnControlRotation = true;

	// Follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Character Configs
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->GravityScale = 1.75f;
	GetCharacterMovement()->AirControl = 0.35f;

	// Components
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	CollisionComponent = CreateDefaultSubobject<UCollisionComponent>(TEXT("CollisionComponent"));
	StatsComponent = CreateDefaultSubobject<UStatsComponent>(TEXT("StatsComponent"));
	FocusComponent = CreateDefaultSubobject<UFocusComponent>(TEXT("FocusComponent"));
}

void ACombatCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// Actions
	// Pressed
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &ACombatCharacter::AttackButtonPressed);
	PlayerInputComponent->BindAction("StrongAttack", IE_Pressed, this, &ACombatCharacter::StrongAttackButtonPressed);
	PlayerInputComponent->BindAction("ChargeAttack", IE_Pressed, this, &ACombatCharacter::ChargeAttackButtonPressed);
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &ACombatCharacter::SprintButtonPressed);
	PlayerInputComponent->BindAction("Focus", IE_Pressed, this, &ACombatCharacter::FocusButtonPressed);
	// Released
	PlayerInputComponent->BindAction("ChargeAttack", IE_Released, this, &ACombatCharacter::ChargeAttackButtonReleased);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &ACombatCharacter::SprintButtonReleased);
	// Axises
	PlayerInputComponent->BindAxis("MoveForward", this, &ACombatCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ACombatCharacter::MoveRight);
	PlayerInputComponent->BindAxis("LookUp", this, &ACombatCharacter::LookUp);
	PlayerInputComponent->BindAxis("Turn", this, &ACombatCharacter::Turn);
}

void ACombatCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (CombatComponent)
	{
		CombatComponent->SetCombatCharacter(this);
	}
	if (CollisionComponent)
	{
		CollisionComponent->SetCharacter(this);
		CollisionComponent->HitActorDelegate.AddDynamic(this, &ACombatCharacter::OnHitActor);
	}
	if (StatsComponent)
	{
		StatsComponent->SetCombatCharacter(this);
		StatsComponent->InitStatValues();
	}
	if (FocusComponent)
	{
		FocusComponent->SetCombatCharacter(this);
	}
}

void ACombatCharacter::BeginPlay()
{
	Super::BeginPlay();

	SpeedMode = ESpeedMode::ESM_Jog;
	GetCharacterMovement()->MaxWalkSpeed = JogSpeed;

	OnTakePointDamage.AddDynamic(this, &ACombatCharacter::OnReceivedPointDamage);

	// HUD
	CombatPlayerController = Cast<ACombatPlayerController>( GetController());
	if (CombatPlayerController)
	{
		CombatPlayerController->CreateCombatWidget();
		CombatPlayerController->AddCombatWidgetToViewport();

		// Update HUD
		CombatPlayerController->
			UpdateHealth_HUD(StatsComponent->GetHealth(), StatsComponent->GetMaxHealth());

		CombatPlayerController->
			UpdateEnergy_HUD(StatsComponent->GetEnergy(), StatsComponent->GetMaxEnergy());
	}

}

UCombatComponent* ACombatCharacter::GetCombat_Implementation() const
{
	return CombatComponent;
}

UCollisionComponent* ACombatCharacter::GetCollision_Implementation() const
{
	return CollisionComponent;
}


// ham nay chay khi ma minh danh trung ai do
void ACombatCharacter::OnHitActor(const FHitResult& HitResult)
{
	// GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, "Hit Actor");

	AActor* HittedActor = HitResult.GetActor();

	if (HittedActor)
	{
		UGameplayStatics::ApplyPointDamage(
			HittedActor,
			GetDamageOfLastAttack(),
			GetActorForwardVector(),
			HitResult,
			GetController(),
			this,
			UDamageType::StaticClass()
		);
	}
}

// ham nay chay khi ma minh bi danh
void ACombatCharacter::OnReceivedPointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy,
	FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection,
	const UDamageType* DamageType, AActor* DamageCauser)
{
	// tru mau
	if (StatsComponent)
	{
		StatsComponent->DecreaseHealth(Damage);
		if (StatsComponent->GetHealth() <= 0.f)
		{
			HandleDead(HitLocation);
		}
		else
		{
			HandleHitted(HitLocation);
		}
	}
}

void ACombatCharacter::HandleHitted(const FVector& HitLocation)
{
	UGameplayStatics::PlaySoundAtLocation(this, HitSound, HitLocation);
	UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitImpact, HitLocation, FRotator());
	PlayAnimMontage(HitReactMontage);

	if (CombatComponent)
	{
		CombatComponent->SetCombatState(ECombatState::ECS_Hitted);
	}
}

void ACombatCharacter::HandleDead(const FVector& HitLocation)
{
	// neu la enemy character thi minh se an thanh mau truoc khi 
	// xu ly doan code o duoi
	UGameplayStatics::PlaySoundAtLocation(this,	DeadSound, HitLocation);
	UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitImpact, HitLocation, FRotator());
	PlayAnimMontage(DeadMontage);

	if (CombatComponent)
	{
		CombatComponent->SetCombatState(ECombatState::ECS_Dead);
	}

	//disable collision capsule component(root component)
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// xoa luon nhan vat ra khoi map sau 1 khoang thoi gian (2 s)
	GetWorldTimerManager().SetTimer(
		DeadTimer,
		this,
		&ACombatCharacter::HandleDeadTimerFinished,
		DeadTime
	);
}

void ACombatCharacter::HandleDeadTimerFinished()
{
	Destroy();
}


// Light Attack
void ACombatCharacter::AttackButtonPressed()
{
	if (CombatComponent)
	{
		if (IsSprinting() == false)
		{
			CombatComponent->RequestAttack(EAttackType::EAT_LightAttack);
		}
		else
		{
			CombatComponent->RequestAttack(EAttackType::EAT_SprintAttack);
		}
	}
}

void ACombatCharacter::StrongAttackButtonPressed()
{
	if (CombatComponent)
	{
		CombatComponent->RequestAttack(EAttackType::EAT_StrongAttack);
	}
}

void ACombatCharacter::ChargeAttackButtonPressed()
{
	// chay timer, doi 1 khoang thoi gian sau do cho nhan vat charge attack
	// chay timer
	GetWorldTimerManager().SetTimer(
		ChargeAttackTimer,
		this,
		&ACombatCharacter::HandleChargeTimerFinish,
		ChargeTime
	);
	// sau khi timer chay xong thi minh moi attack
}

void ACombatCharacter::ChargeAttackButtonReleased()
{
	if (GetWorldTimerManager().IsTimerActive(ChargeAttackTimer))
	{
		GetWorldTimerManager().ClearTimer(ChargeAttackTimer);
	}
}

void ACombatCharacter::HandleChargeTimerFinish()
{
	if (CombatComponent)
	{
		CombatComponent->RequestAttack(EAttackType::EAT_ChargeAttack);
	}
}

void ACombatCharacter::SprintButtonPressed()
{
	if (StatsComponent && StatsComponent->GetEnergy() > 0.f)
	{
		Sprint();
	}
}

void ACombatCharacter::FocusButtonPressed()
{
	if (FocusComponent == nullptr)
	{
		return;
	}

	if (FocusComponent->IsFocusing() == false)
	{
		FocusComponent->Focus();
	}
	else
	{
		FocusComponent->UnFocus();
	}
}

void ACombatCharacter::SprintButtonReleased()
{
	if (SpeedMode == ESpeedMode::ESM_Sprint)
	{
		Jog();
	}

}



void ACombatCharacter::Sprint()
{
	SpeedMode = ESpeedMode::ESM_Sprint;
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void ACombatCharacter::Jog()
{
	SpeedMode = ESpeedMode::ESM_Jog;
	GetCharacterMovement()->MaxWalkSpeed = JogSpeed;
}

void ACombatCharacter::UpdateHealth_HUD(const float& NewHealth, const float& MaxHealth)
{
	if (CombatPlayerController)
	{
		CombatPlayerController->UpdateHealth_HUD(NewHealth, MaxHealth);
	}
	// neu la ke dich thi update mau o tren dau luon
}

void ACombatCharacter::UpdateEnergy_HUD(const float& NewEnergy,const float& MaxEnergy)
{
	if (CombatPlayerController)
	{
		CombatPlayerController->UpdateEnergy_HUD(NewEnergy, MaxEnergy);
	}
}

float ACombatCharacter::GetSpeed()
{
	Velocity = GetVelocity();
	Velocity.Z = 0.f;
	return Velocity.Size();
}

void ACombatCharacter::DecreaseEnergyByAttackType(EAttackType AttackType)
{
	if (StatsComponent)
	{
		StatsComponent->DecreaseEnergyByAttackType(AttackType);
	}
}

bool ACombatCharacter::HasEnoughEnergyForThisAttackType(EAttackType AttackType)
{
	if (StatsComponent == nullptr)
	{
		return false;
	}
	return StatsComponent->HasEnoughEnergyForThisAttackType(AttackType);
}

const FVector ACombatCharacter::GetCameraDirection()
{
	if (FollowCamera == nullptr)
	{
		return FVector();
	}
	return FollowCamera->GetForwardVector();
}

void ACombatCharacter::SetControllerRotation(FRotator NewControllerRotation)
{
	if (CombatPlayerController)
	{
		CombatPlayerController->SetControlRotation(NewControllerRotation);
	}
}

const float ACombatCharacter::GetDamageOfLastAttack()
{
	if (CombatComponent == nullptr)
	{
		return 0.0f;
	}
	return CombatComponent->GetDamageOfLastAttack();
}



void ACombatCharacter::MoveForward(float Value)
{
	// Yaw Pitch Roll
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation = FRotator(0.f, ControlRotation.Yaw, 0.f);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	AddMovementInput(Direction, Value);
}

void ACombatCharacter::MoveRight(float Value)
{
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation = FRotator(0.f, ControlRotation.Yaw, 0.f);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(Direction, Value);
}

void ACombatCharacter::LookUp(float Value)
{
	// Pitch
	AddControllerPitchInput(Value);
}

void ACombatCharacter::Turn(float Value)
{
	// Yaw
	AddControllerYawInput(Value);
}






