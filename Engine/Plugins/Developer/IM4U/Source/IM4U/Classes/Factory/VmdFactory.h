
// Copyright 2015 BlackMa9. All Rights Reserved.
#pragma once


#include "Engine.h"

#include "Factories/Factory.h"
#include "Factories.h"


#include "SkelImport.h"
#include "AnimationUtils.h"

#include "PmxImportUI.h"

#include "VmdImporter.h"

#include "MMD2UE4NameTableRow.h"

#include "VmdFactory.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMMD4UE4_VMDFactory, Log, All)

UCLASS()
class IM4U_API UVmdFactory : public UFactory 
{
	GENERATED_UCLASS_BODY()

	// Begin UFactory Interface
	virtual bool DoesSupportClass(UClass * Class) override;
	virtual UClass* ResolveSupportedClass() override;
	virtual UObject* FactoryCreateBinary(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags Flags,
		UObject* Context,
		const TCHAR* Type,
		const uint8*& Buffer,
		const uint8* BufferEnd,
		FFeedbackContext* Warn,
		bool& bOutOperationCanceled
		) override;
	// End UFactory Interface

	UAnimSequence * ImportAnimations(
		USkeleton* Skeleton, 
		UObject* Outer,
		const FString& Name, 
		//UFbxAnimSequenceImportData* TemplateImportData, 
		//TArray<FbxNode*>& NodeArray
		UDataTable* ReNameTable,
		MMD4UE4::VmdMotionInfo* vmdMotionInfo
		);
	//////////////
	class UPmxImportUI* ImportUI;
	/**********
	* MMD向けベジェ曲線の算出処理
	***********/
	float interpolateBezier(float x1, float y1, float  x2, float y2, float x);
	/*******************
	* 既存AnimSequのアセットにVMDの表情データを追加する処理
	* MMD4Mecanimuとの総合利用向けテスト機能
	**********************/
	UAnimSequence * AddtionalMorphCurveImportToAnimations(
		UAnimSequence* exsistAnimSequ,
		UDataTable* ReNameTable,
		MMD4UE4::VmdMotionInfo* vmdMotionInfo
		);
	/*******************
	* Import Morph Target AnimCurve
	* VMDファイルのデータからMorphtargetのFloatCurveをAnimSeqに取り込む
	**********************/
	bool ImportMorphCurveToAnimSequence(
		UAnimSequence* DestSeq,
		USkeleton* Skeleton,
		UDataTable* ReNameTable,
		MMD4UE4::VmdMotionInfo* vmdMotionInfo
		);
	/*******************
	* Import VMD Animation
	* VMDファイルのデータからモーションデータをAnimSeqに取り込む
	**********************/
	bool ImportVMDToAnimSequence(
		UAnimSequence* DestSeq,
		USkeleton* Skeleton,
		UDataTable* ReNameTable,
		MMD4UE4::VmdMotionInfo* vmdMotionInfo
		);

	/*****************
	* MMD側の名称からTableRowのUE側名称を検索し取得する
	* Return :T is Found
	* @param :ue4Name is Found Row Name
	****************/
	bool FindTableRowMMD2UEName(
		UDataTable* ReNameTable,
		FName mmdName,
		FName * ue4Name
		);
};
