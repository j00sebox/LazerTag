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
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId
#include <string>
#include "Net/UnrealNetwork.h"
#include "Pickup.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

#define __VR__ 0

//////////////////////////////////////////////////////////////////////////
// ALazerTagCharacter

ALazerTagCharacter::ALazerTagCharacter()
{
	m_characterMovement = GetCharacterMovement();
	m_characterMovement->MaxWalkSpeed = f_walkSpeed;

	// timelines are used for certain state transitions
	m_crouchTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("CrouchTimeline"));
	m_camTiltTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("CamTiltTimeline"));
	m_slideTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("SlideTimeline"));
	m_wallRunTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("WallRunTimeline"));

	// bind delegates to the timeline update functions
	CrouchInterp.BindUFunction(this, FName("CrouchTimelineUpdate"));
	CamInterp.BindUFunction(this, FName("CamTiltTimelineUpdate"));
	SlideInterp.BindUFunction(this, FName("SlideTimelineUpdate"));
	WallRunInterp.BindUFunction(this, FName("WallRunUpdate"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, f_standingCapsuleHalfHeight);

	// need event for wall running
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
	//Mesh->SetOwnerNoSee(true);
	_standCollisionParams.AddIgnoredComponent(GetMesh());

#if __VR__ == 0
	// Create a gun mesh component
	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(false);			// otherwise won't be visible in the multiplayer
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

	DOREPLIFETIME(ALazerTagCharacter, pickupSphere);
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
		m_meshStartVec = GetMesh()->GetRelativeLocation();

		m_camEndVec = FVector(m_camStartVec.X, m_camEndVec.Y, m_camStartVec.Z - f_cameraZOffset);
		m_meshEndVec = FVector(m_meshStartVec.X, m_meshStartVec.Y, m_meshStartVec.Z + f_meshZOffset);

		// general settings for timeline
		m_crouchTimeline->SetLooping(false);
		m_crouchTimeline->SetIgnoreTimeDilation(true);

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
				// collect pickup and deactivate
				obj->WasCollected();
				obj->SetActive(false);
			}
		}
	}
}

void ALazerTagCharacter::CrouchTimelineUpdate(float value)
{
	// need to reduce the capsule size for the duration of the crouch/slide
	GetCapsuleComponent()->SetCapsuleHalfHeight(FMath::Lerp(f_standingCapsuleHalfHeight, f_standingCapsuleHalfHeight*f_capsuleHeightScale, value));
	
	// the mesh needs to be offset as well so it doesn't clip through the floor
	FVector currentMeshPos = GetMesh()->GetRelativeLocation();
	GetMesh()->SetRelativeLocation(FVector(currentMeshPos.X, currentMeshPos.Y, FMath::Lerp(m_meshStartVec.Z, m_meshEndVec.Z, value)));

	// lower camera view
	FVector currentCamPos = FirstPersonCameraComponent->GetRelativeLocation();
	FirstPersonCameraComponent->SetRelativeLocation(FVector(currentCamPos.X, currentCamPos.Y, FMath::Lerp(m_camStartVec.Z, m_camEndVec.Z, value)));
}

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
				m_characterMovement->Velocity = FVector(m_wallRunDir.X, m_wallRunDir.Y, 0) * m_characterMovement->GetModifiedMaxSpeed();
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

void ALazerTagCharacter::CapsuleHit(const FHitResult& Hit)
{
	if (!OnWall())
	{
		if (m_characterMovement->IsFalling() && WallRunnable(Hit.ImpactNormal))
		{
 			m_wallRunDir = FindWallRunDir(-Hit.ImpactNormal);

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

	if(surfaceNormal.Z < 0)
		return false;

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
		return FVector::CrossProduct(wallNormal, -FVector::UpVector);
	else
		return FVector::CrossProduct(wallNormal, FVector::UpVector);
}

FVector ALazerTagCharacter::FindLaunchVelocity()
{
	FVector launchVelocity;

	if (OnWall())
	{
		if (CurrentSide == EWallSide::LEFT)
		{
			launchVelocity = FVector::CrossProduct(m_wallRunDir, -FVector::UpVector);
		}
		else
		{
			launchVelocity = FVector::CrossProduct(m_wallRunDir, FVector::UpVector);
		}

		launchVelocity += FVector::UpVector;

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
  	bool correctKey = false;

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

	// input to collect pickups
	InputComponent->BindAction("Pickup", IE_Pressed, this, &ALazerTagCharacter::CollectPickup);

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
	f_forwardMovement = Value;

	if (Value != 0.0f && CurrentMoveState != EMovementStates::SLIDING)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);

		if (CurrentMoveState == EMovementStates::CROUCHING && CanStand())
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

		if (CurrentMoveState == EMovementStates::CROUCHING && CanStand())
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

		if (OnWall())
			EndWallRun();
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

	SetMovementState(EMovementStates::CROUCHING);
}

void ALazerTagCharacter::Stand()
{
	b_crouchKeyDown = false;

	if (b_sprintKeyDown)
	{
		SetMovementState(EMovementStates::SPRINTING);
	}
	else
	{
		SetMovementState(EMovementStates::WALKING);
	}
}

void ALazerTagCharacter::Sprint()
{
	if (!b_sprintKeyDown)
	{
		b_sprintKeyDown = true;
	}
	
	SetMovementState(EMovementStates::SPRINTING);
}

void ALazerTagCharacter::StopSprint()
{
	b_sprintKeyDown = false;

	SetMovementState(EMovementStates::WALKING);
}

void ALazerTagCharacter::SetMovementState(EMovementStates newState)
{
	switch (CurrentMoveState)
	{
		case EMovementStates::WALKING:
		{
			
			if (newState == EMovementStates::SPRINTING && CanSprint())
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
			else if (newState == EMovementStates::WALKING)
				CurrentMoveState = newState;
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
					CurrentMoveState = newState;
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

	f_camRollRotation = f_camRollRotationOffRight;

	m_slideDir = GetActorForwardVector();

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

void ALazerTagCharacter::BeginWallRun()
{
	m_characterMovement->AirControl = 1.f;

	m_characterMovement->GravityScale = 0.f;

	m_characterMovement->SetPlaneConstraintNormal(FVector(0, 0, 1.f));

	b_isWallRunning = true;

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
		

	SetMovementState(EMovementStates::SPRINTING);

	m_camTiltTimeline->Play();
	m_wallRunTimeline->Play();
}

void ALazerTagCharacter::EndWallRun()
{
	m_characterMovement->AirControl = .05f;

	m_characterMovement->GravityScale = 1.f;

	m_characterMovement->SetPlaneConstraintNormal(FVector(0, 0, 0));

	b_isWallRunning = false;

	m_camTiltTimeline->Reverse();
	m_wallRunTimeline->Stop();
}

void ALazerTagCharacter::SetMaxWalkSpeed()
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
	
	return  /* !m_characterMovement->IsFalling()  && **/ CanStand();

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