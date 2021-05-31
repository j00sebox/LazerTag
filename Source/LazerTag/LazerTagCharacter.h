// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Runtime/Engine/Classes/Components/TimelineComponent.h"
#include "UObject/WeakObjectPtr.h"
#include "LazerTagCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class USceneComponent;
class UCameraComponent;
class UMotionControllerComponent;
class UAnimMontage;
class USoundBase;
class UTimelineComponent;
class UCurveFloat;

UCLASS(config=Game)
class ALazerTagCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	USkeletalMeshComponent* Mesh1P;

	/** Gun mesh: 1st person view (seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	USkeletalMeshComponent* FP_Gun;

	/** Location on gun mesh where projectiles should spawn. */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	USceneComponent* FP_MuzzleLocation;

	/** Gun mesh: VR view (attached to the VR controller directly, no arm, just the actual gun) */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	USkeletalMeshComponent* VR_Gun;

	/** Location on VR gun mesh where projectiles should spawn. */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	USceneComponent* VR_MuzzleLocation;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Motion controller (right hand) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UMotionControllerComponent* R_MotionController;

	/** Motion controller (left hand) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UMotionControllerComponent* L_MotionController;

public:
	ALazerTagCharacter();

protected:
	virtual void BeginPlay();

public:
	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

	/** Gun muzzle's offset from the characters location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	FVector GunOffset;

	/** Projectile class to spawn */
	UPROPERTY(EditDefaultsOnly, Category=Projectile)
	TSubclassOf<class ALazerTagProjectile> ProjectileClass;

	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	USoundBase* FireSound;

	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	UAnimMontage* FireAnimation;

	/** Whether to use motion controller location for aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	uint8 bUsingMotionControllers : 1;

	enum class MovementStates
	{
		WALKING = 0,
		CROUCHING,
		SPRINTING,
		SLIDING,
	};
	
	enum class WallSide
	{
		RIGHT = 0,
		LEFT,
	};

	enum class StopWallRunningReason
	{
		FALLOFF = 0,
		JUMP,
	};

	void BeginCrouch();

	void EndCrouch();

	void BeginSlide();

	void EndSlide();

	FScriptDelegate OnCapsuleHit;

	// delegate that is binded with CrouchTimelineUpdate
	FOnTimelineFloat CrouchInterp;

	// delegate that is used to tilt the camera
	FOnTimelineFloat CamInterp;

	// delegate that is binded with SlideTimelineUpdate
	FOnTimelineFloat SlideInterp;


	UFUNCTION()
	void CapsuleHit();


	UFUNCTION()
	void CrouchTimelineUpdate(float value);

	UFUNCTION()
	void CamTiltTimelineUpdate(float value);

	UFUNCTION()
	void SlideTimelineUpdate(float value);


private:

	MovementStates CurrentMoveState = MovementStates::WALKING;
	bool b_crouchKeyDown;
	bool b_sprintKeyDown;
	
	/* General Movement */
	float f_capsuleHeightScale = .5f;
	float f_cameraZOffset = 64.f;
	float f_standingCapsuleHalfHeight = 96.f;
	float f_camXRotationOff = -10.f;
	const float f_walkSpeed = 600.f;
	const float f_sprintSpeed = 1200.f;
	const float f_crouchSpeed = 300.f;

	/* Wall Running */
	FVector m_wallRunDir;
	bool b_isWallRunning;
	const int i_maxJumps = 2;
	int i_jumpsLeft = i_maxJumps;
	WallSide CurrentSide;

	/* Timelines */
	UTimelineComponent* m_crouchTimeline;
	UTimelineComponent* m_slideTimeline;
	UTimelineComponent* m_camTiltTimeline;

	UPROPERTY(EditAnywhere, Category = "Timeline")
	UCurveFloat* fCrouchCurve;

	UPROPERTY(EditAnywhere, Category = "Timeline")
	UCurveFloat* fSlideCurve;

	UPROPERTY()
	FVector m_camStartVec;

	UPROPERTY()
	FVector m_camEndVec;

	UCharacterMovementComponent* m_characterMovement;

	FCollisionQueryParams _standCollisionParams;

protected:
	
	/** Fires a projectile. */
	void OnFire();

	/** Resets HMD orientation and position in VR. */
	void OnResetVR();

	/** Handles moving forward/backward */
	void MoveForward(float Val);

	/** Handles stafing movement, left and right */
	void MoveRight(float Val);

	/**
	 * Called via input to turn at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	void Jump() override;

	void Landed(const FHitResult& Hit) override;

	void Crouch();
	void Stand();

	void Sprint();
	void StopSprint();

	void SetMovementState(MovementStates newState);

	void SetMaxWalkSpeed();

	FVector CalculateFloorInfluence();

	bool CanSprint();

	bool CanStand();

	bool OnWall();

	bool UseJump();

	void ResetJump();

	bool WallRunnable();

	FVector FindWallRunDir();

	FVector FindLaunchVelocity();

	bool CanWallRun();

	FVector GetHorizontalVel();

	void SetHorizontalVel();

	void ClampHorizontalVel();
	
protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

public:
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

