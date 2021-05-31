// Copyright Epic Games, Inc. All Rights Reserved.

#include "LazerTagCharacter.h"
#include "LazerTagProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// ALazerTagCharacter

ALazerTagCharacter::ALazerTagCharacter()
{
	m_characterMovement = GetCharacterMovement();
	m_characterMovement->MaxWalkSpeed = f_walkSpeed;

	m_crouchTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("CrouchTimeline"));
	m_camTiltTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("CamTiltTimeline"));
	m_slideTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("SlideTimeline"));

	CrouchInterp.BindUFunction(this, FName("CrouchTimelineUpdate"));
	CamInterp.BindUFunction(this, FName("CamTiltTimelineUpdate"));
	SlideInterp.BindUFunction(this, FName("SlideTimelineUpdate"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, f_standingCapsuleHalfHeight);

	OnCapsuleHit.BindUFunction(this, FName("CapsuleHit"));

	GetCapsuleComponent()->OnComponentHit.Add(OnCapsuleHit);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, f_cameraZOffset)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

	_standCollisionParams.AddIgnoredComponent(Mesh1P);
	_standCollisionParams.AddIgnoredComponent(GetCapsuleComponent());
	_standCollisionParams.AddIgnoredActor(this);

	// Create a gun mesh component
	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(false);			// otherwise won't be visible in the multiplayer
	FP_Gun->bCastDynamicShadow = false;
	FP_Gun->CastShadow = false;
	// FP_Gun->SetupAttachment(Mesh1P, TEXT("GripPoint"));
	FP_Gun->SetupAttachment(RootComponent);

	_standCollisionParams.AddIgnoredComponent(FP_Gun);

	FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FP_MuzzleLocation->SetupAttachment(FP_Gun);
	FP_MuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 0.0f, 10.0f);

	// Note: The ProjectileClass and the skeletal mesh/anim blueprints for Mesh1P, FP_Gun, and VR_Gun 
	// are set in the derived blueprint asset named MyCharacter to avoid direct content references in C++.

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
	//bUsingMotionControllers = true;
}

void ALazerTagCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	// check if curve asset is valid
	if (fCrouchCurve)
	{
		/* set up crouch timeline */

		// adds the curve to the timeline and hooks it up to the delegate
		m_crouchTimeline->AddInterpFloat(fCrouchCurve, CrouchInterp, FName("CrouchPro"));

		m_camStartVec = FirstPersonCameraComponent->GetRelativeLocation();

		m_camEndVec = FVector(m_camStartVec.X, m_camEndVec.Y, m_camStartVec.Z - f_cameraZOffset);

		// general settings for timeline
		m_crouchTimeline->SetLooping(false);
		m_crouchTimeline->SetIgnoreTimeDilation(true);

		m_camTiltTimeline->AddInterpFloat(fCrouchCurve, CamInterp, FName("TiltPro"));

		// general settings for timeline
		m_camTiltTimeline->SetLooping(false);
		m_camTiltTimeline->SetIgnoreTimeDilation(true);
	}

	if (fSlideCurve)
	{
		/* set up slide timeline */
		m_slideTimeline->AddInterpFloat(fSlideCurve, SlideInterp, FName("SlideTick"));

		// general settings for timeline
		m_slideTimeline->SetIgnoreTimeDilation(true);
	}

	//Attach gun mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("GripPoint"));

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

void ALazerTagCharacter::CrouchTimelineUpdate(float value)
{
	GetCapsuleComponent()->SetCapsuleHalfHeight(FMath::Lerp(f_standingCapsuleHalfHeight, f_standingCapsuleHalfHeight*f_capsuleHeightScale, value));

	FVector currentCamPos = FirstPersonCameraComponent->GetRelativeLocation();
	FirstPersonCameraComponent->SetRelativeLocation(FVector(currentCamPos.X, currentCamPos.Y, FMath::Lerp(m_camStartVec.Z, m_camEndVec.Z, value)));
}

void ALazerTagCharacter::CamTiltTimelineUpdate(float value)
{
	AController* controller = GetController();

	FRotator currentCamRot = controller->GetControlRotation();

	currentCamRot = FRotator(currentCamRot.Pitch, currentCamRot.Yaw, FMath::Lerp(0.f, f_camXRotationOff, value));

	controller->SetControlRotation(currentCamRot);
}

void ALazerTagCharacter::SlideTimelineUpdate(float value)
{
	FVector force = CalculateFloorInfluence();

	m_characterMovement->AddForce(force * 150000);

	FVector vel = GetVelocity();

	if (vel.Size() > f_sprintSpeed)
	{
		vel.Normalize();

		m_characterMovement->Velocity = (vel * f_sprintSpeed);
	}
	else if (vel.Size() < f_crouchSpeed)
	{
		m_characterMovement->Velocity = FVector(0, 0, 0);
		SetMovementState(MovementStates::WALKING);
	}
}


//////////////////////////////////////////////////////////////////////////
// Input

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

void ALazerTagCharacter::CapsuleHit()
{
	return;
}

//void ALazerTagCharacter::OnLanded()
//{
//	ResetJump();
//}

void ALazerTagCharacter::ResetJump()
{
	i_jumpsLeft = i_maxJumps;
}

bool ALazerTagCharacter::WallRunnable()
{
	return false;
}

FVector ALazerTagCharacter::FindWallRunDir()
{
	return FVector();
}

FVector ALazerTagCharacter::FindLaunchVelocity()
{
	FVector launchVelocity;

	if (OnWall())
	{
		if (CurrentSide == WallSide::LEFT)
		{
			launchVelocity = FVector::CrossProduct(m_wallRunDir, FVector::UpVector);
		}
		else
		{
			launchVelocity = FVector::CrossProduct(m_wallRunDir, -FVector::UpVector);
		}

		launchVelocity.Normalize();
	}
	else
	{
		launchVelocity = FVector::UpVector;
	}

	launchVelocity *= m_characterMovement->JumpZVelocity;
	
	return launchVelocity;
}

bool ALazerTagCharacter::CanWallRun()
{
	return false;
}

FVector ALazerTagCharacter::GetHorizontalVel()
{
	return FVector();
}

void ALazerTagCharacter::SetHorizontalVel()
{
}

void ALazerTagCharacter::ClampHorizontalVel()
{
}

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
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ALazerTagCharacter::Crouch);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &ALazerTagCharacter::Stand);

}

void ALazerTagCharacter::OnFire()
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
				World->SpawnActor<ALazerTagProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
			}
		}
	}

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
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);

		if (CurrentMoveState == MovementStates::CROUCHING && CanStand())
		{
			EndCrouch();
		}
	}
}

void ALazerTagCharacter::MoveRight(float Value)
{
	if (Value != 0.0f && CurrentMoveState != MovementStates::SLIDING)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);

		if (CurrentMoveState == MovementStates::CROUCHING && CanStand())
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
	
	if (UseJump())
	{
		LaunchCharacter(FindLaunchVelocity(), false, true);
	}
	

	return;
}

void ALazerTagCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	ResetJump();
}

void ALazerTagCharacter::Crouch()
{

	b_crouchKeyDown = true;

	SetMovementState(MovementStates::CROUCHING);
}

void ALazerTagCharacter::Stand()
{
	b_crouchKeyDown = false;

	SetMovementState(MovementStates::WALKING);
}

void ALazerTagCharacter::Sprint()
{
	if (!b_sprintKeyDown)
	{
		b_sprintKeyDown = true;

		SetMovementState(MovementStates::SPRINTING);
	}
}

void ALazerTagCharacter::StopSprint()
{
	b_sprintKeyDown = false;

	SetMovementState(MovementStates::WALKING);
}

void ALazerTagCharacter::SetMovementState(MovementStates newState)
{
	switch (CurrentMoveState)
	{
		case MovementStates::WALKING:
		{
			
			if (newState == MovementStates::SPRINTING && CanSprint())
				CurrentMoveState = newState;
			else if (newState == MovementStates::CROUCHING)
			{
				BeginCrouch();
				CurrentMoveState = newState;
			}
				
			break;
		}
		case MovementStates::SPRINTING:
		{
			if (newState == MovementStates::CROUCHING)
			{
				CurrentMoveState = MovementStates::SLIDING;
				BeginCrouch();
				BeginSlide();
			}
			else if (newState == MovementStates::WALKING)
				CurrentMoveState = newState;
			break;
		}
		case MovementStates::CROUCHING:
		{
			if (newState == MovementStates::SPRINTING && CanSprint())
			{
				EndCrouch();
				CurrentMoveState = newState;
			}
			else if (newState == MovementStates::WALKING && CanStand())
			{
				EndCrouch();
				CurrentMoveState = newState;
			}
				
			break;
		}
		case MovementStates::SLIDING:
		{
			if (newState == MovementStates::WALKING)
			{
				if (CanStand())
				{
					EndCrouch();
					CurrentMoveState = newState;
				}
				else
					CurrentMoveState = MovementStates::CROUCHING;

				EndSlide();
			}
			break;
		}
	}

	SetMaxWalkSpeed();
}

void ALazerTagCharacter::BeginCrouch()
{
	m_crouchTimeline->Play();
}

void ALazerTagCharacter::EndCrouch()
{
	m_crouchTimeline->Reverse();
}

void ALazerTagCharacter::BeginSlide()
{
	m_characterMovement->GroundFriction = 0.f;

	m_characterMovement->BrakingDecelerationWalking = 1000.f;

	m_slideTimeline->Play();
	m_camTiltTimeline->Play();
}

void ALazerTagCharacter::EndSlide()
{
	m_characterMovement->GroundFriction = 8.f;

	m_characterMovement->BrakingDecelerationWalking = 2048.f;

	m_slideTimeline->Stop();
	m_camTiltTimeline->Reverse();
}

void ALazerTagCharacter::SetMaxWalkSpeed()
{
	switch (CurrentMoveState)
	{
		case MovementStates::WALKING:
			m_characterMovement->MaxWalkSpeed = f_walkSpeed;
			break;
		case MovementStates::SPRINTING:
			m_characterMovement->MaxWalkSpeed = f_sprintSpeed;
			break;
		case MovementStates::CROUCHING:
			m_characterMovement->MaxWalkSpeed = f_crouchSpeed;
			break;
		case MovementStates::SLIDING:
			break;
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
	
	return !m_characterMovement->IsFalling() && CanStand();

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