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
class USphereComponent;

UENUM(blueprinttype)
enum class EMovementStates : uint8
{
	WALKING = 0		UMETA(DisplayName="WALKING"),
	CROUCHING		UMETA(DisplayName = "CROUCHING"),
	SPRINTING		UMETA(DisplayName = "SPRINTING"),
	SLIDING			UMETA(DisplayName = "SLIDING"),
};

// this state represents whats side the wall is on relative to the player while wall running
UENUM(blueprinttype)
enum class EWallSide : uint8
{
	RIGHT = 0,
	LEFT,
};

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

	/** Pickup Sphere */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	USphereComponent* pickupSphere;

	/** Motion controller (right hand) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UMotionControllerComponent* R_MotionController;

	/** Motion controller (left hand) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UMotionControllerComponent* L_MotionController;

public:
	ALazerTagCharacter();

	// required network setup
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& outLifetimeProps) const override;

protected:
	virtual void BeginPlay();

	// entry to pickup logic
	UFUNCTION(blueprintcallable, category = "Pickup")
	void CollectPickup();

	// called on server to process collection of pickups
	UFUNCTION(reliable, server, withvalidation)
	void Server_CollectPickup();
	void Server_CollectPickup_Implementation();
	bool Server_CollectPickup_Validate();

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

	UPROPERTY(editanywhere, blueprintreadonly, category = Gameplay)
	float f_forwardMovement;

	UPROPERTY(replicated, editanywhere, blueprintreadonly, category = Gameplay)
	EMovementStates CurrentMoveState = EMovementStates::WALKING;

	// starts the crouching timeline
	void BeginCrouch();

	// reverses crouching timeline
	void EndCrouch();

	void BeginSlide();

	void EndSlide();

	void BeginWallRun();

	void EndWallRun();

	// delegate invoked by the OnComponentHit event
	FScriptDelegate OnCapsuleHit;

	// delegate that is binded with CrouchTimelineUpdate
	FOnTimelineFloat CrouchInterp;

	// delegate that is used to tilt the camera
	FOnTimelineFloat CamInterp;

	// delegate that is binded with SlideTimelineUpdate
	FOnTimelineFloat SlideInterp;

	// delegate that is binded with WallRunUpdate
	FOnTimelineFloat WallRunInterp;


	UFUNCTION()
	/* event triggers everytime player comes into contact with a surface*/
	void CapsuleHit(const FHitResult& Hit);

	UFUNCTION()
	/* perform crouching events such as lowering camera */
	void CrouchTimelineUpdate(float value);

	UFUNCTION()
	/* event that gradually tilts camera while wall running or sliding */
	void CamTiltTimelineUpdate(float value);

	UFUNCTION()
	/* slide events */
	void SlideTimelineUpdate(float value);

	UFUNCTION()
	/* wall running events */
	void WallRunUpdate(float value);

	// curves used for the timelines
	UPROPERTY(editanywhere, category = "Timeline")
	UCurveFloat* fCrouchCurve;

	UPROPERTY(editanywhere, category = "Timeline")
	UCurveFloat* fTickCurve;

	UFUNCTION(blueprintpure, category = "Shield")
	int GetRemainingCharges() const;

	/*
	* This updates the players amount of shield. 
	* Either adding or removing a charge based on the circumstances
	* @param delta This is the amount of charges to add/remove
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Shield")
	void UpdateCharges(int delta);

protected:

	// initial shield charges
	UPROPERTY(replicated, editanywhere, blueprintreadwrite, category = "Shield")
	int i_shieldCharges = 0;

	// max shield charges player can have
	UPROPERTY(EditAnywhere, Category = "Shield")
	int i_maxShieldCharges = 2;

	UPROPERTY( replicated )
	bool b_crouchKeyDown;

	UPROPERTY( replicated )
	bool b_sprintKeyDown;
	
	/* General Movement */
	const float f_walkSpeed = 600.f;
	const float f_sprintSpeed = 1200.f;
	const float f_crouchSpeed = 300.f;
	FVector m_slideDir;

	float f_sideMovement;
	float f_capsuleHeightScale = .5f;
	float f_cameraZOffset = 64.f;

	UPROPERTY( editanywhere, blueprintreadwrite, category = Character, meta = (allowprivateaccess = "true"))
	float f_meshZOffset = 50.0f;

	UPROPERTY(editanywhere, category = "Capsule")
	float f_standingCapsuleHalfHeight = 96.f;

	UPROPERTY(editanywhere, category = "Capsule")
	float f_crouchCapsuleHalfHeight = 55.f;

	/* Rotation Info */
	float f_camRollRotationOffLeft = 10.f;
	float f_camRollRotationOffRight = -10.f;
	float f_camRollRotation;
	float f_meshPitchRotationOffLeft = 35.f;
	float f_meshPitchRotationOffRight = -35.f;
	float f_meshPitchRotation;

	/* Wall Running */
	FVector m_wallRunDir;

	UPROPERTY(replicated)
	bool b_isWallRunning = false;

	const int i_maxJumps = 2;

	UPROPERTY(replicated)
	int i_jumpsLeft = i_maxJumps;

	UPROPERTY(replicated)
	EWallSide CurrentSide;

	/* Timelines */
	UTimelineComponent* m_crouchTimeline;
	UTimelineComponent* m_slideTimeline;
	UTimelineComponent* m_camTiltTimeline;
	UTimelineComponent* m_wallRunTimeline;

	UPROPERTY()
	FVector m_camStartVec;

	UPROPERTY()
	FVector m_camEndVec;

	FVector m_meshStartVec;
	FVector m_meshEndVec;

	UCharacterMovementComponent* m_characterMovement;

	FCollisionQueryParams _standCollisionParams;

	UPROPERTY(replicated, visibleanywhere, blueprintreadonly, category = "Pickup", meta = ( allowprivateaccess = "true" ) )
	float f_pickupSphereRadius;
	
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

	/* Overrides the ACharacter definition of Jump. This will allow the player to double jump. */
	void Jump() override;

	UFUNCTION(reliable, server)
	void Server_Jump();
	void Server_Jump_Implementation();

	/* 
	* Overrides the ACharacter definition of Landed. This is called once the character reaches the ground. 
	* Once that happens the jumps need to be reset.
	*/
	void Landed(const FHitResult& Hit) override;

	/* Binded to a key and performs the crouching adjustments needed. */
	void Crouch();

	UFUNCTION(reliable, server)
	void Server_Crouch();
	void Server_Crouch_Implementation();

	/* Binded to a key and reverses all the changes made by Crouch */
	void Stand();

	UFUNCTION(reliable, server)
	void Server_Stand();
	void Server_Stand_Implementation();

	/* Binded to a key. Makes the player move faster. */
	void Sprint();

	UFUNCTION(reliable, server)
	void Server_Sprint();
	void Server_Sprint_Implementation();

	/* Stops the sprint to move into a new movement state */
	void StopSprint();

	UFUNCTION(reliable, server)
	void Server_StopSprint();
	void Server_StopSprint_Implementation();

	/* Sets movement state of player. This is called whenever a state change is requested by one of the previous functions. */
	void SetMovementState(EMovementStates newState);

	UFUNCTION(reliable, server)
	void Server_SetMovementState(EMovementStates newState);
	void Server_SetMovementState_Implementation(EMovementStates newState);

	/* Different speeds depending on what the player's movement state is so this sets it. */
	void SetMaxWalkSpeed();

	/* 
	* Gets the direction and magnitude of the slide direction. For instance this would return a larger vector if the floor was steeper .
	* @returns FVector - The amount of influence the floor has on the player in vector form.
	*/
	FVector CalculateFloorInfluence();

	/*
	* Checks if the player currently has the ability to sprint based on numerous factors.
	* @returns bool	- true: if the player can infact sprint
	*				  false: player is not able to sprint at this time
	*/
	bool CanSprint();

	/*
	* Checks if the player currently has the ability to stand based on if there is anything above them blocking them.
	* @returns bool	- true: if there is nothing blocking the player from standing
	*				  false: the player is currently being restricted by something above them
	*/
	bool CanStand();

	/*
	* Checks if the player currently is wall running.
	* @returns bool	- true: if the player is wall running
	*				  false: player is not wall running
	*/
	bool OnWall();

	/*
	* First checks if the player has any jumps left and then uses one up. This check is not done if a player is wall running.
	* @returns bool	- true: if jump was successful and the jump count was decremented 
	*				  false: player has no jumps left
	*/
	bool UseJump();

	/* Resets the players amount of jumps to whatever the max jumps number is */
	void ResetJump();

	/*
	* Determines if the surface the player collides with can be wall run.
	* @param - A vector that represents the normal of the surface that the player has collided with
	* @returns bool - true: the angle between the surface and the ground is below the walkable angle and can be wall run
	*				  false: the surface is not steep enough to be considered a runnable wall
	*/
	bool WallRunnable(FVector surfaceNormal);

	/*
	* Once the wall is determined to be runnable the direction the player travels must be caluclated.
	* @param - A vector that represents the normal of the wall that is currently being run on
	* @returns FVector - the new direction that the player is travelling along the wall
	*/
	FVector FindWallRunDir(FVector wallNormal);

	/*
	* The direction to jump differs depending if the player is on a wall or not.
	* This calculates what direction the jump velocity should be in.
	* @returns FVector - A vector representing the velocity of the jump
	*/
	FVector FindLaunchVelocity();

	/*
	* This determines if the player is holding down the proper keys needed to wall run.
	* They must be holding down the forward key as well as the left or right key depending on what side the wall is on.
	* @returns bool - true: player is holding down correct keys and can start/continue wall running
	*				  false: player is not holding down proper keys and wall running should be stopped
	*/
	bool CanWallRun();
	
protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

public:
	/** Returns Mesh1P subobject **/
	FORCEINLINE USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	FORCEINLINE UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
	/** Returns the sphere that determines if the player can pickup an object */
	FORCEINLINE USphereComponent* GetPickupSphere() const { return pickupSphere; }

};

