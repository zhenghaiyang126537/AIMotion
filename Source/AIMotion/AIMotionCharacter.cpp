// Copyright Epic Games, Inc. All Rights Reserved.

#include "AIMotionCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "DrawDebugHelpers.h"

//////////////////////////////////////////////////////////////////////////
// AAIMotionCharacter

AAIMotionCharacter::AAIMotionCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
	FName  PoseableMeshName(TEXT("CharacterMesh1"));
	PoseableMesh = CreateOptionalDefaultSubobject<UPoseableMeshComponent>(PoseableMeshName);
	if (PoseableMesh)
	{
		PoseableMesh->AlwaysLoadOnClient = true;
		PoseableMesh->AlwaysLoadOnServer = true;
		PoseableMesh->bOwnerNoSee = false;
		PoseableMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
		PoseableMesh->bCastDynamicShadow = true;
		PoseableMesh->bAffectDynamicIndirectLighting = true;
		PoseableMesh->PrimaryComponentTick.TickGroup = TG_PrePhysics;
		PoseableMesh->SetupAttachment(GetCapsuleComponent());
		static FName MeshCollisionProfileName(TEXT("PoseableMesh"));
		PoseableMesh->SetCollisionProfileName(MeshCollisionProfileName);
		PoseableMesh->SetGenerateOverlapEvents(false);
		PoseableMesh->SetCanEverAffectNavigation(false);
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AAIMotionCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AAIMotionCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AAIMotionCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AAIMotionCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AAIMotionCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AAIMotionCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AAIMotionCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AAIMotionCharacter::OnResetVR);
}


void AAIMotionCharacter::OnResetVR()
{
	// If AIMotion is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in AIMotion.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AAIMotionCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void AAIMotionCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void AAIMotionCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AAIMotionCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AAIMotionCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
		GetNearHigh();
	}
}

void AAIMotionCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}
TMap<FName, FTransform> AAIMotionCharacter::GetBoneAllTransfrom()
{
	TMap<FName, FTransform> Res;
	if (nullptr == PoseableMesh)
	{
		return Res;
	}
	TArray<FName> BoneNameArray;
	PoseableMesh->GetBoneNames(BoneNameArray);
	for (size_t i = 0; i < BoneNameArray.Num(); i++)
	{
		Res.Add(BoneNameArray[i], PoseableMesh->GetBoneTransformByName(BoneNameArray[i], EBoneSpaces::WorldSpace));
	}
	return Res;
}
void AAIMotionCharacter::SetBoneAllTransfrom(const TMap<FName, FTransform>& TransformMap)
{
	if (nullptr == PoseableMesh)
	{
		return;
	}
	for (auto it = TransformMap.CreateConstIterator(); it; ++it)
	{
		PoseableMesh->SetBoneTransformByName(it->Key, it->Value, EBoneSpaces::WorldSpace);
	}
}
TArray<FVector> AAIMotionCharacter::GetNearHigh()
{
	TArray<FVector> Res;
	FVector CurLocation = GetActorLocation();
	FRotator CurRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, CurRotation.Yaw - 90, 0);
	// get right vector 
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	FCollisionShape Shape;
	Shape.SetBox(FVector(20.f, 20.f, 20.f));
	TArray<FHitResult> OutHits;
	FCollisionObjectQueryParams QueryParamsObj(ECollisionChannel::ECC_WorldStatic);

	for (int32 i = 0; i < 6; ++i)
	{
		FVector StartLocation = CurLocation + Direction * i * 100;
		FVector EndLocation = StartLocation;
		EndLocation.Z -= 200;
		bool bHitSuccess = GWorld->SweepMultiByObjectType(OutHits, StartLocation, EndLocation, FQuat(0, 0, 0, 1), QueryParamsObj, Shape);
		FVector HitPoint = StartLocation;
		if (bHitSuccess)
		{
			for (int32 j = 0; j < OutHits.Num(); j++)
			{
				float CurZ = OutHits[j].ImpactPoint.Z;

				if (HitPoint.Z > CurZ)
				{
					HitPoint = OutHits[j].ImpactPoint;
				}

				UE_LOG(LogMemory, Log, TEXT("[Steven.Han]StepExportMapData, Line=%d, CurZ=%f"), __LINE__, CurZ);

			}
		}
		Res.Add(HitPoint);
		FVector DrowLocation = HitPoint;
		DrowLocation.Z += 100;
		DrowTip(DrowLocation, HitPoint);
	}
	return Res;
}
void AAIMotionCharacter::DrowTip(FVector StartPoint, FVector EndPoint)
{
	DrawDebugSphere(GWorld, EndPoint, 10.f, 10.f, FColor::Yellow, false, 0.1f, 0, 2.0f);
}