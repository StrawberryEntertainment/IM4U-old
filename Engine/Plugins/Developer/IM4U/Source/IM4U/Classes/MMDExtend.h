// Copyright 2015 BlackMa9. All Rights Reserved.
#pragma once

#include "Engine.h"
#include "Factories/Factory.h"
//#include "Factories/FbxFactory.h"
#include "UnrealEd.h"
#include "Factories.h"
#include "BusyCursor.h"
#include "SSkeletonWidget.h"
#include "SkelImport.h"


#include "MMDExtend.generated.h"



/**********************
* MMD Extend Info : Ik info
***********************/
// �h�j�����N���
USTRUCT()
struct FMMD_IKLINK
{
	GENERATED_USTRUCT_BODY()

	int		BoneIndex;							// Link bone index ( for skeleton bone index ,use ik func)

	UPROPERTY(EditAnywhere, meta = (ToolTip = "ue4 link bone name") )
	FName	BoneName;							// Link Bone Name

	UPROPERTY(EditAnywhere, meta = (ToolTip = "If enabled, rotation limite, 0:OFF, 1:ON "))
	uint32	RotLockFlag:1;						// ��]����( 0:OFF 1:ON )

	UPROPERTY(EditAnywhere, meta = (ToolTip = "Rotation angle limit Euler[dig:-180~180] min"))
	FVector	RotLockMin;							// ��]�����A����[x,y,z]

	UPROPERTY(EditAnywhere, meta = (ToolTip = "Rotation angle limit Euler[dig:-180~180] max"))
	FVector	RotLockMax;							// ��]�����A���[x,y,z]

	FMMD_IKLINK()
	{
		BoneIndex = -1;
		RotLockFlag = 0;
	};
};

// �h�j���
USTRUCT()
struct FMMD_IKInfo
{
	GENERATED_USTRUCT_BODY()

	// IK�v�Z�\�t���O
	bool	checkIKIndex;

	int		IKBoneIndexVMDKey;			// IK target bone index vmd key index
	int		IKBoneIndex;					// IK target bone index ( use ik func. ref skeleton.)

	UPROPERTY(EditAnywhere, meta = (ToolTip = "ue4 IK (this) bone name"))
	FName	IKBoneName;

	int		TargetBoneIndex;					// IK target bone index ( use ik func. ref skeleton.)

	UPROPERTY(EditAnywhere, meta = (ToolTip = "ue4 target bone name"))
	FName	TargetBoneName;						// IK Target Bone Name 

	UPROPERTY(EditAnywhere, meta = (ToolTip = "CCD-IK loop count") )
	int32		LoopNum;							// IK�v�Z�̃��[�v��

	UPROPERTY(EditAnywhere, meta = (ToolTip = "CCD-IK unit angle[dig]"))
	float	RotLimit;							// �v�Z���ӂ�̐����p�x

	UPROPERTY(EditAnywhere, meta = (ToolTip = "CCD-IK link IK info"))
	TArray<FMMD_IKLINK> ikLinkList;				// �h�j�����N���

	FMMD_IKInfo()
	{
		checkIKIndex = false;
		IKBoneIndexVMDKey = -1;
		IKBoneIndex = -1;
		TargetBoneIndex = -1;
		LoopNum = 0;
		RotLimit = 0;
	};
};
/**************************************************************/

/**********************
* MMD Extend Info : Asset class
***********************/
//UCLASS(hidecategories = Object)
UCLASS()
class UMMDExtend : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	////////////////////////////////////
	// mmd target model name
	UPROPERTY(EditAnywhere, Category = Header )
		FString ModelName;
	// mmd model comment 
	UPROPERTY(EditAnywhere, Category = Header)
		FString ModelComment;

	////////////////////////////////////
	// MMD-IK-Info is used to generate the AnimSequence form VMD file.
	UPROPERTY(EditAnywhere, Category = IK)
		TArray<FMMD_IKInfo> IkInfoList;	
	
	//TBD::�p�r�s��
	//	bool CanEditChange( const UProperty* InProperty ) const;
private:

};