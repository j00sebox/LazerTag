// Copyright Epic Games, Inc. All Rights Reserved.

#include "LazerTagCharacter.h"
#include "LazerTagProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/SpringArmComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId
#include <string>
#include "Net/UnrealNetwork.h"
#include "Pickup.h"
#include "Shield.h"
#include "PState.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

#define __VR__ 0
#define __SERVER__ (ALazerTagCharacter::GetLocalRole() == ROLE_Authority)

//////////////////////////////////////////////////////////////////////////
// ALazerTagCharacter

ALazerTagCharacter::ALazerTagCharacter()
{
	m_characterMovement = GetCharacterMovement();
	m_characterMovement->MaxWalkSpeed = f_walkSpeed;

	// timelines are used for certain state transitions
	m_camTiltTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("CamTiltTimeline"));
	m_slideTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("SlideTimeline"));
	m_wallRunTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("WallRunTimeline"));

	// bind delegates to the timeline update functions
	CamInterp.BindUFunction(this, FName("CamTiltTimelineUpdate"));
	SlideInterp.BindUFunction(this, FName("SlideTimelineUpdate"));
	WallRunInterp.BindUFunction(this, FName("WallRunUpdate"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, f_standingCapsuleHalfHeight);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	springArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CamerSpringArm"));
	springArm->SetupAttachment(GetCapsuleComponent());
	springArm->bUsePawnControlRotation = true;
	springArm->bEnableCameraLag = true;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(springArm);
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, f_cameraZOffset)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = false;

	// set base sphere radius
	f_pickupSphereRadius = 200.f;

	// create pickup sphere
	pickupSphere = CreateAbstractDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	pickupSphere->SetupAttachment(RootComponent);
	pickupSphere->SetSphereRadius(f_pickupSphereRadius);

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;

	// these params are used for tracing a ray to check if the player can stand
	_standCollisionParams.AddIgnoredComponent(Mesh1P);
	_standCollisionParams.AddIgnoredComponent(GetCapsuleComponent());
	_standCollisionParams.AddIgnoredActor(this);
	
	// Mesh is the multiplayer mesh that other players can see
	GetMesh()->SetOwnerNoSee(true);
	_standCollisionParams.AddIgnoredComponent(GetMesh());

	// Gun that can be seen in multiplayer
	MP_Gun = CreateAbstractDefaultSubobject<USkeletalMeshComponent>(TEXT("MP_Gun"));
	MP_Gun->SetOwnerNoSee(true);
	MP_Gun->SetupAttachment(RootComponent);

#if __VR__ == 0
	// Create a gun mesh component
	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(true);			// otherwise won't be visible in the multiplayer
	FP_Gun->bCastDynamicShadow = false;
	FP_Gun->CastShadow = false;
	FP_Gun->SetupAttachment(RootComponent);

	_standCollisionParams.AddIgnoredComponent(FP_Gun);

	FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FP_MuzzleLocation->SetupAttachment(FP_Gun);

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 0.0f, 10.0f);

	// Note: The ProjectileClass and the skeletal mesh/anim blueprints for Mesh1P, FP_Gun, and VR_Gun 
	// are set in the derived blueprint asset named MyCharacter to avoid direct content references in C++.

#else
	// Create VR Controllers.
	R_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("R_MotionController"));
	R_MotionController->MotionSource = FXRMotionControllerBase::RightHandSourceId;
	R_MotionController->SetupAttachment(RootComponent);
	L_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("L_MotionController"));
	L_MotionController->SetupAttachment(RootComponent);

	// Create a gun and attach it to the right-hand VR controller.
	// Create a gun mesh component
	VR_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VR_Gun"));
	VR_Gun->SetOnlyOwnerSee(false);			// otherwise won't be visible in the multiplayer
	VR_Gun->bCastDynamicShadow = false;
	VR_Gun->CastShadow = false;
	VR_Gun->SetupAttachment(R_MotionController);
	VR_Gun->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	_standCollisionParams.AddIgnoredComponent(VR_Gun);

	VR_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("VR_MuzzleLocation"));
	VR_MuzzleLocation->SetupAttachment(VR_Gun);
	VR_MuzzleLocation->SetRelativeLocation(FVector(0.000004, 53.999992, 10.000000));
	VR_MuzzleLocation->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));		// Counteract the rotation of the VR gun model.

	// Uncomment the following line to turn motion controllers on by default:
	bUsingMotionControllers = true;
#endif

}

void ALazerTagCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// replicate variables that are marked
	DOREPLIFETIME(ALazerTagCharacter, pickupSphere);
	DOREPLIFETIME(ALazerTagCharacter, i_shieldCharges);
	DOREPLIFETIME(ALazerTagCharacter, CurrentMoveState);
	DOREPLIFETIME(ALazerTagCharacter, i_jumpsLeft);
	DOREPLIFETIME(ALazerTagCharacter, b_isWallRunning);
	DOREPLIFETIME(ALazerTagCharacter, b_crouchKeyDown);
	DOREPLIFETIME(ALazerTagCharacter, b_sprintKeyDown);
	DOREPLIFETIME_CONDITION(ALazerTagCharacter, f_camStartZ, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ALazerTagCharacter, f_camRollRotation, COND_OwnerOnly);
	DOREPLIFETIME(ALazerTagCharacter, CurrentSide);
	DOREPLIFETIME(ALazerTagCharacter, f_meshPitchRotation);
}

void ALazerTagCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	if (__SERVER__)
	{
		f_camStartZ = springArm->GetRelativeLocation().Z;
	}

	// check if curve asset is valid
	if (fCrouchCurve)
	{
		/* set up crouch timeline */

		m_camTiltTimeline->AddInterpFloat(fCrouchCurve, CamInterp, FName("TiltPro"));

		// general settings for timeline
		m_camTiltTimeline->SetLooping(false);
		m_camTiltTimeline->SetIgnoreTimeDilation(true);
	}

	if (fTickCurve)
	{
		/* set up slide timeline */
		m_slideTimeline->AddInterpFloat(fTickCurve, SlideInterp, FName("SlideTick"));

		// general settings for timeline
		m_slideTimeline->SetLooping(true);
		m_slideTimeline->SetIgnoreTimeDilation(true);

		m_wallRunTimeline->AddInterpFloat(fTickCurve, WallRunInterp, FName("WallRunTick"));

		m_wallRunTimeline->SetLooping(true);
		m_wallRunTimeline->SetIgnoreTimeDilation(true);
	}

	//Attach gun mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("GripPoint"));
	MP_Gun->AttachToComponent(GetMesh(), FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("gunSocket"));

	// Show or hide the two versions of the gun based on whether or not we're using motion controllers.
	if (bUsingMotionControllers)
	{
		//VR_Gun->SetHiddenInGame(false, true);
		Mesh1P->SetHiddenInGame(true, true);
	}
	else
	{
		//VR_Gun->SetHiddenInGame(true, true);
		Mesh1P->SetHiddenInGame(false, true);
	}
}

void ALazerTagCharacter::CollectPickup()
{
	// ask server to collect pickups
	Server_CollectPickup();
}

bool ALazerTagCharacter::Server_CollectPickup_Validate()
{
	return true;
}

void ALazerTagCharacter::Server_CollectPickup_Implementation()
{
	if (GetLocalRole() == ROLE_Authority)
	{
		// get all overlapping actors and store
		TArray<AActor*> overlapActors;

		pickupSphere->GetOverlappingActors(overlapActors);

		// find an APickup onbject 
		for(int i = 0; i < overlapActors.Num(); i++)
		{
			APickup* const obj = Cast<APickup>(overlapActors[i]);

			if( obj != NULL && !obj->IsPendingKill() && obj->IsActive() )
			{
				if (AShield* const shield = Cast<AShield>(obj))
				{
					UpdateCharges(shield->chargePerPickup);
				}

				// collect pickup and deactivate
				obj->Server_PickedUpBy(this);
				obj->SetActive(false);
			}
		}
	}
}

/******************************TIMELINE FUNCTIONS******************************/

void ALazerTagCharacter::CamTiltTimelineUpdate(float value)
{
	AController* controller = GetController();

	FRotator currentCamRot = controller->GetControlRotation();

	currentCamRot = FRotator(currentCamRot.Pitch, currentCamRot.Yaw, FMath::Lerp(0.f, f_camRollRotation, value));

	if (b_isWallRunning)
	{
		FRotator currentMeshRot = GetMesh()->GetRelativeRotation();
		currentMeshRot = FRotator(FMath::Lerp(0.f, f_meshPitchRotation, value), currentMeshRot.Yaw, currentMeshRot.Roll);
		GetMesh()->SetRelativeRotation(currentMeshRot);
	}

	controller->SetControlRotation(currentCamRot);
}

void ALazerTagCharacter::WallRunUpdate(float value)
{

	FHitResult hit;

	// while the player can still wall run
  	if (CanWallRun())
	{
		FVector start = GetActorLocation();

		FVector intoWall;

		// check what side of the player the wall is on to get a vector that goes into the wall
		if (CurrentSide == EWallSide::LEFT)
			intoWall = FVector::CrossProduct(m_wallRunDir, FVector::UpVector);
		else
			intoWall = FVector::CrossProduct(m_wallRunDir, -FVector::UpVector);

		// scale vector so it is large enough to actually reach the wall
		intoWall *= 100;

		FVector end = start + intoWall;

		// if there is no hit then the player is no longer on a wall 
		if(!GetWorld()->LineTraceSingleByChannel(hit, start, end, ECC_WorldStatic, _standCollisionParams))
 			EndWallRun();
		else
		{
			EWallSide prevWallSide = CurrentSide;

  			m_wallRunDir = FindWallRunDir(hit.ImpactNormal);

			if (prevWallSide == CurrentSide)
			{
				Server_UpdateVelocity(m_wallRunDir);
			}
			else
			{
				EndWallRun();
			}
		}
	}
	else
	{
		EndWallRun();
	}
}

void ALazerTagCharacter::SlideTimelineUpdate(float value)
{
	FVector force = CalculateFloorInfluence();

	m_characterMovement->AddForce(force * 150000);

	FVector vel = GetVelocity();

	float debug = vel.Size();

	if (vel.Size() > f_sprintSpeed)
	{
		vel.Normalize();

		m_characterMovement->Velocity = (vel * f_sprintSpeed);
	}
	else if (vel.Size() < f_crouchSpeed)
	{
		m_characterMovement->Velocity = FVector(0, 0, 0);

		SetMovementState(EMovementStates::WALKING);
	}
}

/******************************TIMELINE FUNCTIONS END******************************/

int ALazerTagCharacter::GetRemainingCharges() const
{
	return i_shieldCharges;
}

void ALazerTagCharacter::UpdateCharges(int delta)
{
	if (__SERVER__)
	{
		int res = i_shieldCharges + delta;

		// increase or decrease shield charges
		if ((res) < 0)
		{
			i_shieldCharges = 0;
		}
		else if ((res) > i_maxShieldCharges)
		{
			i_shieldCharges = i_maxShieldCharges;
		}
		else
		{
			i_shieldCharges += delta;
		}
		
	}
}

bool ALazerTagCharacter::OnWall()
{
	return b_isWallRunning;
}

bool ALazerTagCharacter::UseJump()
{
	if (!OnWall())
	{
		if (i_jumpsLeft > 0)
		{
			i_jumpsLeft--;

			return true;
		}
		else
			return false;
	}

	return true;
		
}

void ALazerTagCharacter::CapsuleHit(FVector impactNormal)
{
	if (!OnWall())
	{
		if (m_characterMovement->IsFalling() && WallRunnable(impactNormal))
		{
 			m_wallRunDir = FindWallRunDir(impactNormal);

   			if (CanWallRun())
			{
				BeginWallRun();
			}
		}
	}
}

void ALazerTagCharacter::ResetJump()
{
	i_jumpsLeft = i_maxJumps;
}

bool ALazerTagCharacter::WallRunnable(FVector surfaceNormal)
{

	/*if(surfaceNormal.Z < 0)
		return false;*/

	surfaceNormal.Normalize();

   	FVector newVec = FVector(surfaceNormal.X, surfaceNormal.Y, 0);

	newVec.Normalize();

    float res = FVector::DotProduct(surfaceNormal, newVec);

	float angle = FMath::RadiansToDegrees( FMath::Acos(res) );

 	if (angle < m_characterMovement->GetWalkableFloorAngle())
	{
		return true;
	}

	return false;
}

FVector ALazerTagCharacter::FindWallRunDir(FVector wallNormal)
{

	wallNormal.Normalize();
	
	FVector2D normal = FVector2D(wallNormal);
	FVector2D right = FVector2D(GetActorRightVector());

	FVector test = GetActorLocation() + wallNormal;

	float res = FVector2D::DotProduct(normal, right);

	if (res < 0)
	{
		CurrentSide = EWallSide::RIGHT;
	}
	else
	{
		CurrentSide = EWallSide::LEFT;
	}

	if(CurrentSide == EWallSide::RIGHT)
		return FVector::CrossProduct(FVector::UpVector, wallNormal);
	else
		return FVector::CrossProduct(-FVector::UpVector, wallNormal);
}

// this find the direction the player should jump off the wall depending on the slope
FVector ALazerTagCharacter::FindLaunchVelocity()
{
	FVector launchVelocity;

	if (OnWall())
	{
		// the side the wall is on is used to determine which vector should be crossed
		if (CurrentSide == EWallSide::LEFT)
		{
			launchVelocity = FVector::CrossProduct(m_wallRunDir, -FVector::UpVector);
		}
		else
		{
			launchVelocity = FVector::CrossProduct(m_wallRunDir, FVector::UpVector);
		}

		// adds a little upwards motion to the juump
		launchVelocity += FVector::UpVector;

		// final direction to jump
		launchVelocity.Normalize();
	}
	else
	{
		// if the player is not on a wall then it can just be up
		launchVelocity = FVector::UpVector;
	}

	launchVelocity *= m_characterMovement->JumpZVelocity;
	
	return launchVelocity;
}

// this checks if the player is holding down the correct keys in order to wall run
bool ALazerTagCharacter::CanWallRun()
{
  	bool correctKey = false;

	// needs to be going forwards can holding either the left or right key depending what side the wall is on
  	if (f_sideMovement > 0.1f && CurrentSide == EWallSide::RIGHT)
	{
		correctKey = true;
	}
	else if (f_sideMovement < -0.1f && CurrentSide == EWallSide::LEFT)
	{
		correctKey = true;
	}

	return (f_forwardMovement > 0.1f) && correctKey;
}

/******************************INPUT******************************/

void ALazerTagCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ALazerTagCharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ALazerTagCharacter::OnFire);


	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &ALazerTagCharacter::OnResetVR);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &ALazerTagCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ALazerTagCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ALazerTagCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ALazerTagCharacter::LookUpAtRate);

	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &ALazerTagCharacter::Sprint);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &ALazerTagCharacter::StopSprint);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ALazerTagCharacter::CCrouch);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &ALazerTagCharacter::Stand);

	// input to collect pickups
	InputComponent->BindAction("Pickup", IE_Pressed, this, &ALazerTagCharacter::CollectPickup);

}

void ALazerTagCharacter::OnFire_Implementation()
{
	// try and fire a projectile
	if (ProjectileClass != nullptr)
	{
		UWorld* const World = GetWorld();
		if (World != nullptr)
		{
			if (bUsingMotionControllers)
			{
				const FRotator SpawnRotation = VR_MuzzleLocation->GetComponentRotation();
				const FVector SpawnLocation = VR_MuzzleLocation->GetComponentLocation();
				World->SpawnActor<ALazerTagProjectile>(ProjectileClass, SpawnLocation, SpawnRotation);
			}
			else
			{
				const FRotator SpawnRotation = GetControlRotation();
				// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
				const FVector SpawnLocation = ((FP_MuzzleLocation != nullptr) ? FP_MuzzleLocation->GetComponentLocation() : GetActorLocation()) + SpawnRotation.RotateVector(GunOffset);

				//Set Spawn Collision Handling Override
				FActorSpawnParameters ActorSpawnParams;
				ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

				// spawn the projectile at the muzzle
				ALazerTagProjectile* newProjectile = World->SpawnActor<ALazerTagProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);

				if (newProjectile != nullptr)
				{
					newProjectile->SetShooter(this);
				}

			}
		}
	}

	Client_OnFire();
}

// plays the hurt animation
void ALazerTagCharacter::OnHit()
{
	if (hitAnimation != nullptr)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(hitAnimation, 1.f);
		}
	}
}

void ALazerTagCharacter::Client_OnFire_Implementation()
{
	// try and play the sound if specified
	if (FireSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}

	// try and play a firing animation if specified
	if (FireAnimation != nullptr)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}

void ALazerTagCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}


void ALazerTagCharacter::MoveForward(float Value)
{	
	f_forwardMovement = Value;
	
	if (Value != 0.0f && CurrentMoveState != EMovementStates::SLIDING)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);

		if (!b_crouchKeyDown && CurrentMoveState == EMovementStates::CROUCHING && CanStand())
		{
			EndCrouch();

			if (b_sprintKeyDown)
			{
				SetMovementState(EMovementStates::SPRINTING);
			}
		}
	}
}

void ALazerTagCharacter::MoveRight(float Value)
{
	f_sideMovement = Value;

	if (Value != 0.0f && CurrentMoveState != EMovementStates::SLIDING && !b_isWallRunning)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);

		if (!b_crouchKeyDown && CurrentMoveState == EMovementStates::CROUCHING && CanStand())
		{
			EndCrouch();
		}
	}
}

void ALazerTagCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ALazerTagCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void ALazerTagCharacter::Jump()
{
	Server_Jump();
}

// server is repsonsible for making sure the jump is decremented properly
void ALazerTagCharacter::Server_Jump_Implementation()
{
	if(__SERVER__)
	{
		if (UseJump())
		{
			LaunchCharacter(FindLaunchVelocity(), false, true);

			if (OnWall())
				EndWallRun();
		}
	}
}

void ALazerTagCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	ResetJump();
}

void ALazerTagCharacter::CCrouch()
{
	Server_Crouch();

	SetMovementState(EMovementStates::CROUCHING);
}

void ALazerTagCharacter::Server_Crouch_Implementation()
{
	if (__SERVER__)
	{
		b_crouchKeyDown = true;
	}
}

void ALazerTagCharacter::Stand()
{
	Server_Stand();

	if (b_sprintKeyDown)
	{
		SetMovementState(EMovementStates::SPRINTING);
	}
	else
	{
		SetMovementState(EMovementStates::WALKING);
	}
}

void ALazerTagCharacter::Server_Stand_Implementation()
{
	if (__SERVER__)
	{
		b_crouchKeyDown = false;
	}
}

void ALazerTagCharacter::Sprint()
{
	Server_Sprint();

	Server_SetMovementState(EMovementStates::SPRINTING);
}

void ALazerTagCharacter::Server_Sprint_Implementation()
{
	if (GetLocalRole() == ROLE_Authority)
	{
		if (!b_sprintKeyDown)
		{
			b_sprintKeyDown = true;
		}
	}
}

void ALazerTagCharacter::StopSprint()
{
	Server_StopSprint();

	Server_SetMovementState(EMovementStates::WALKING);
}

void ALazerTagCharacter::Server_StopSprint_Implementation()
{
	if (GetLocalRole() == ROLE_Authority)
	{
		b_sprintKeyDown = false;
	}
}

void ALazerTagCharacter::SetMovementState(EMovementStates newState)
{
	Server_SetMovementState(newState);
}

void ALazerTagCharacter::Server_SetMovementState_Implementation(EMovementStates newState)
{
	if (GetLocalRole() == ROLE_Authority)
	{
		switch (CurrentMoveState)
		{
			case EMovementStates::WALKING:
			{

				if (newState == EMovementStates::SPRINTING && (CanSprint() || b_isWallRunning))
				{
					CurrentMoveState = newState;
				}
				else if (newState == EMovementStates::CROUCHING)
				{
					BeginCrouch();
					CurrentMoveState = newState;
				}

				break;
			}
			case EMovementStates::SPRINTING:
			{
				if (newState == EMovementStates::CROUCHING && !b_isWallRunning)
				{
					CurrentMoveState = EMovementStates::SLIDING;
					BeginCrouch();
					BeginSlide();
				}
				else if (newState == EMovementStates::WALKING && !b_isWallRunning)
				{
					CurrentMoveState = newState;
				}
				break;
			}
			case EMovementStates::CROUCHING:
			{
				if (newState == EMovementStates::SPRINTING && CanSprint())
				{
					EndCrouch();
					CurrentMoveState = newState;
				}
				else if (newState == EMovementStates::WALKING && CanStand())
				{
					EndCrouch();
					CurrentMoveState = newState;
				}

				break;
			}
			case EMovementStates::SLIDING:
			{
				if (newState == EMovementStates::WALKING)
				{
					if (CanStand())
					{
						EndCrouch();
						if (!b_sprintKeyDown)
						{
							CurrentMoveState = newState;
						}
						else
						{
							CurrentMoveState = EMovementStates::SPRINTING;
						}
						
					}
					else
						CurrentMoveState = EMovementStates::CROUCHING;

					EndSlide();
				}
				break;
			}
		}

		SetMaxWalkSpeed();
	}
}

void ALazerTagCharacter::BeginCrouch_Implementation()
{
	GetCharacterMovement()->bWantsToCrouch = true;
}

void ALazerTagCharacter::EndCrouch_Implementation()
{
	GetCharacterMovement()->bWantsToCrouch = false;
}

void ALazerTagCharacter::BeginSlide()
{
	m_characterMovement->GroundFriction = 0.f;

	m_characterMovement->BrakingDecelerationWalking = 1000.f;

	f_camRollRotation = f_camRollRotationOffRight;

	m_slideDir = GetActorForwardVector();

	OnCamTilt();

	m_slideTimeline->Play();
}

void ALazerTagCharacter::EndSlide()
{
	m_characterMovement->GroundFriction = 8.f;

	m_characterMovement->BrakingDecelerationWalking = 2048.f;

	OnCamUnTilt();

	m_slideTimeline->Stop();
}

void ALazerTagCharacter::OnCamTilt_Implementation()
{
	CamTiltStart();
}

void ALazerTagCharacter::OnCamUnTilt_Implementation()
{
	CamTiltReverse();
}

void ALazerTagCharacter::HitPlayer_Implementation()
{
	ShowHitMarker();

	// try and play the sound if specified
	if (hitMarkerSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, hitMarkerSound, GetActorLocation());
	}
}

void ALazerTagCharacter::BeginWallRun()
{
	if (CurrentSide == EWallSide::LEFT)
	{
		f_camRollRotation = f_camRollRotationOffLeft;
		f_meshPitchRotation = f_meshPitchRotationOffLeft;
	}
	else
	{
		f_camRollRotation = f_camRollRotationOffRight;
		f_meshPitchRotation = f_meshPitchRotationOffRight;
	}

	Server_EnableWallRun();

	SetMovementState(EMovementStates::SPRINTING);

	CamTiltStart();

	MeshTilt();

	ResetJump();

	m_wallRunTimeline->Play();
}

void ALazerTagCharacter::Server_EnableWallRun_Implementation()
{
	m_characterMovement->AirControl = 1.f;

	m_characterMovement->GravityScale = 0.f;

	m_characterMovement->SetPlaneConstraintNormal(FVector(0, 0, 1.f));

	b_isWallRunning = true;
}

void ALazerTagCharacter::Server_UpdateVelocity_Implementation(FVector dir)
{
	m_characterMovement->Velocity = FVector(dir.X, dir.Y, 0) * m_characterMovement->GetModifiedMaxSpeed();
}

void ALazerTagCharacter::EndWallRun()
{
	Server_DisableWallRun();

	CamTiltReverse();

	MeshTiltReverse();

	if(!b_sprintKeyDown)
	{
		SetMovementState(EMovementStates::WALKING);
	}
	
	m_wallRunTimeline->Stop();
}

void ALazerTagCharacter::Server_DisableWallRun_Implementation()
{
	m_characterMovement->AirControl = .05f;

	m_characterMovement->GravityScale = 1.f;

	m_characterMovement->SetPlaneConstraintNormal(FVector(0, 0, 0));

	b_isWallRunning = false;

}

// test to see if another player is in line of sight
void ALazerTagCharacter::PlayerNameVisible_Implementation()
{
	FHitResult hit;

	FRotator rot = GetControlRotation();

	FVector start = FP_MuzzleLocation->GetComponentLocation();
	FVector end = start + (FP_MuzzleLocation->GetForwardVector() * 5000);

	if (GetWorld()->LineTraceSingleByChannel(hit, start, end, ECC_WorldStatic, _standCollisionParams) && hit.Actor != this)
	{
		// if there is another player then their name can be displayed above their head
		if (ALazerTagCharacter* target = Cast< ALazerTagCharacter>(hit.Actor))
		{
			if (prevTarget != nullptr)
			{
				if (target != prevTarget)
				{
					RemoveName(prevTarget);
				}
			}


			if (target->GetPlayerState() != nullptr)
			{
				DisplayName(target, target->GetPlayerState()->GetPlayerName());
			}
			else
			{
				DisplayName(target, TEXT("Sal T."));
			}
			

			/*if ( APState* pstate = Cast<APState>(target->GetPlayerState()) )
			{
				DisplayName(target, pstate->GetName());
			}*/

			

			// have to keep track of the previous target to know when to stop showing the name
			prevTarget = target;
		}
		else if (prevTarget != nullptr)
		{
			RemoveName(prevTarget);
		}
	}
	else if (prevTarget != nullptr)
	{
		RemoveName(prevTarget);
	}
}

void ALazerTagCharacter::SetMaxWalkSpeed()
{

	if (__SERVER__)
	{
		switch (CurrentMoveState)
		{
			case EMovementStates::WALKING:
				m_characterMovement->MaxWalkSpeed = f_walkSpeed;
				break;
			case EMovementStates::SPRINTING:
				m_characterMovement->MaxWalkSpeed = f_sprintSpeed;
				break;
			case EMovementStates::CROUCHING:
				m_characterMovement->MaxWalkSpeed = f_crouchSpeed;
				break;
			case EMovementStates::SLIDING:
				break;
		}
	}
}

FVector ALazerTagCharacter::CalculateFloorInfluence()
{

	FVector normal = m_characterMovement->CurrentFloor.HitResult.Normal;

	if (normal.Z == FVector::UpVector.Z)
	{
		return FVector(0, 0, 0);
	}

	FVector rightInfluence = FVector::CrossProduct(normal, FVector::UpVector);

	FVector slopeInfluence = FVector::CrossProduct(normal, rightInfluence);

	slopeInfluence.Normalize();

	float slidyness = FVector::DotProduct(normal, FVector::UpVector);

	slopeInfluence *= slidyness;

	return slopeInfluence;
}

bool ALazerTagCharacter::CanSprint()
{
	if (!b_sprintKeyDown)
		return false;
	
	return  !m_characterMovement->IsFalling()  &&  CanStand();

}

bool ALazerTagCharacter::CanStand()
{
	if (b_crouchKeyDown)
		return false;

	FVector start = GetActorLocation();
	float halfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	start.Z -= halfHeight;
	FVector end = start;
	end.Z += f_standingCapsuleHalfHeight * 2;
	FHitResult hit;

	return !GetWorld()->LineTraceSingleByChannel(hit, start, end, ECC_WorldStatic, _standCollisionParams);
}

/******************************INPUT END******************************/