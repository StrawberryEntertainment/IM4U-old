﻿// Copyright 2015 BlackMa9. All Rights Reserved.

#include "../IM4UPrivatePCH.h"

#include "Factory/VmdFactory.h"
#include "VmdImporter.h"

#include "UnrealEd.h"
#include "SkelImport.h"
#include "AnimationUtils.h"
#include "ObjectTools.h"
#include "AssetRegistryModule.h"

#include "PmxImporter.h"
#include "PmxImportUI.h"


#include "Factory/VmdImportOption.h"


DEFINE_LOG_CATEGORY(LogMMD4UE4_VMDFactory)
/////////////////////////////////////////////////////////
//prototype ::from dxlib 
// Ｘ軸を中心とした回転行列を作成する
void CreateRotationXMatrix(FMatrix *Out, float Angle);
// 回転成分だけの行列の積を求める( ３×３以外の部分には値も代入しない )
void MV1LoadModelToVMD_CreateMultiplyMatrixRotOnly(FMatrix *Out, FMatrix *In1, FMatrix *In2);
// 角度制限を判定する共通関数 (subIndexJdgの判定は割りと不明…)
void CheckLimitAngle(
	const FVector& RotMin,
	const FVector& RotMax,
	FVector * outAngle, //target angle ( in and out param)
	bool subIndexJdg //(ik link index < ik loop temp):: linkBoneIndex < ikt
	);
///////////////////////////////////////////////////////

UVmdFactory::UVmdFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	SupportedClass = NULL;
	//SupportedClass = UPmxFactory::StaticClass();
	Formats.Empty();

	Formats.Add(TEXT("vmd;vmd animations"));

	bCreateNew = false;
	bText = false;
	bEditorImport = true;

}

void UVmdFactory::PostInitProperties()
{
	Super::PostInitProperties();

	ImportUI = NewObject<UPmxImportUI>(this, NAME_None, RF_NoFlags);
}

bool UVmdFactory::DoesSupportClass(UClass* Class)
{
	return (Class == UVmdFactory::StaticClass());
}

UClass* UVmdFactory::ResolveSupportedClass()
{
	return UVmdFactory::StaticClass();
}

UObject* UVmdFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const uint8*&		Buffer,
	const uint8*		BufferEnd,
	FFeedbackContext*	Warn,
	bool&				bOutOperationCanceled
)
{
	MMD4UE4::VmdMotionInfo vmdMotionInfo;

	if (vmdMotionInfo.VMDLoaderBinary(Buffer, BufferEnd) == false)
	{
		UE_LOG(LogMMD4UE4_VMDFactory, Error,
			TEXT("VMD Import Cancel:: vmd data load faile."));
		return NULL;
	}

	/////////////////////////////////////////
	UAnimSequence* LastCreatedAnim = NULL;
	USkeleton* Skeleton = NULL;
	PMXImportOptions* ImportOptions = NULL;
	//////////////////////////////////////

	/***************************************
	* IM4U 暫定版 仕様通知メッセージ
	****************************************/
	if (true)
	{
		//モデル読み込み後の警告文表示：コメント欄
		FText TitleStr = FText::Format(LOCTEXT("ImportReadMe_Generic_Dbg", "{0} 制限事項"), FText::FromString("IM4U Plugin"));
		const FText MessageDbg
			= FText(LOCTEXT("ImportReadMe_Generic_Dbg_Comment",
			"次のImportOption用Slateはまだ実装途中です。\n\
			現時点で有効なパラメータは、\n\
			::Skeleton Asset(必須::Animation関連付け先)\n\
			::Animation Asset(NULL以外で既存AssetにMorphのみ追加する処理を実行。NULLでBoneとMorph含む新規Asset作成)\n\
			::DataTable(MMD2UE4Name) Asset(任意：NULL以外で読み込み時にBoneとMorphNameをMMD=UE4で読み替えてImportを実行する。事前にCSV形式でImportか新規作成しておく必要あり。)\n\
			::MmdExtendAsse(任意：NULL以外でVMDからAnimSeqアセット生成時にExtendからIK情報を参照し計算する際に使用。事前にモデルインポートか手動にてアセット生成しておくこと。)\n\
			です。\n\
			\n\
			注意：新規Asset生成はIKなど未対応の為非推奨。追加Morphのみ対応。"
			)
			);
		FMessageDialog::Open(EAppMsgType::Ok, MessageDbg, &TitleStr);
	}

	//Test VMD Slate（現在はコメントアウト。削除予定）
	if(false)
	{
		TSharedPtr<SWindow> ParentWindow;
		// Check if the main frame is loaded.  When using the old main frame it may not be.
		if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
		{
			IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
			ParentWindow = MainFrame.GetParentWindow();
		}

		TSharedPtr<SVMDImportOptions> ImportOptionsWindow;

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("VMDOptionsWindowTitle", "VMD Targfet DataTable Options"))
			.SizingRule(ESizingRule::Autosized);

		Window->SetContent
			(
			SAssignNew(ImportOptionsWindow, SVMDImportOptions)
			.WidgetWindow(Window)
			);

		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	}
	/***************************************
	* VMD取り込み時の警告表示
	****************************************/
	if (true)
	{
		//モデル読み込み後の警告文表示：コメント欄
		FText TitleStr = FText(LOCTEXT("ImportVMD_TargetModelInfo", "警告[ImportVMD_TargetModelInfo]"));
		const FText MessageDbg
			= FText::Format(LOCTEXT("ImportVMD_TargetModelInfo_Comment",
			"注意：モーションデータ取り込み情報：：\n\
			\n\
			本VMDは、「{0}」用に作成されたファイルです。\n\
			\n\
			モデル向けモーションの場合、同じボーン名のデータのみ取り込まれます。\n\
			モデル側のボーン名と異なる名称の同一ボーンが含まれる場合、\n\
			事前に変換テーブル(MMD2UE4NameTableRow)を作成し、\n\
			InportOption画面にて指定することで取り込むことが可能です。"
			)
			, FText::FromString(vmdMotionInfo.ModelName)
			);
		FMessageDialog::Open(EAppMsgType::Ok, MessageDbg, &TitleStr);
	}
	/////////////////////////////////////
	// factory animation asset from vmd data.
	////////////////////////////////////
	if (vmdMotionInfo.keyCameraList.Num() == 0)
	{
		//カメラアニメーションでない場合
		FPmxImporter* PmxImporter = FPmxImporter::GetInstance();

		EPMXImportType ForcedImportType = PMXIT_StaticMesh;
		bool bOperationCanceled = false;
		bool bIsPmxFormat = true;
		// show Import Option Slate
		bool bImportAll = false;
		ImportUI->bIsObjImport = false;//anim mode
		ImportUI->OriginalImportType = EPMXImportType::PMXIT_Animation;
		ImportOptions
			= GetImportOptions(
				PmxImporter,
				ImportUI,
				true,//bShowImportDialog, 
				InParent->GetPathName(),
				bOperationCanceled,
				bImportAll,
				ImportUI->bIsObjImport,//bIsPmxFormat,
				bIsPmxFormat,
				ForcedImportType
				);
		if (ImportOptions)
		{
			Skeleton = ImportUI->Skeleton;
			////////////////////////////////////
			if (!ImportOptions->AnimSequenceAsset)
			{
				//create AnimSequence Asset from VMD
				LastCreatedAnim = ImportAnimations(
					Skeleton,
					InParent,
					Name.ToString(),
					ImportUI->MMD2UE4NameTableRow,
					ImportUI->MmdExtendAsset,
					&vmdMotionInfo
					);
			}
			else
			{
				//TBD::OptionでAinimSeqが選択されていない場合、終了
				// add morph curve only to exist ainimation
				LastCreatedAnim = AddtionalMorphCurveImportToAnimations(
					ImportOptions->AnimSequenceAsset,//UAnimSequence* exsistAnimSequ,
					ImportUI->MMD2UE4NameTableRow,
					&vmdMotionInfo
					); 
			}
		}
		else
		{
			UE_LOG(LogMMD4UE4_VMDFactory, Warning, 
				TEXT("VMD Import Cancel"));
		}
	}
	else
	{
		//カメラアニメーションのインポート処理
		//今後実装予定？
		UE_LOG(LogMMD4UE4_VMDFactory, Warning,
			TEXT("VMD Import Cancel::Camera root... not impl"));

		LastCreatedAnim = NULL;
		//未実装なのでコメントアウト
#ifdef IM4U_FACTORY_MATINEEACTOR_VMD
		////////////////////////
		// Import Optionを設定するslateに関しては必要ない認識。
		//

		// アセットの生成
		//AMatineeActor* InMatineeActor = NULL;
		//ここにアセットの基本生成処理もしくは既存アセットの再利用処理を実装すること。

		//targetのアセットに対しカメラアニメーションをインポートさせる
		if (ImportMatineeSequence(
			InMatineeActor,
			vmdMotionInfo
			) == false)
		{
			//import error
		}
#endif 
	}
	return LastCreatedAnim;
};

UAnimSequence * UVmdFactory::ImportAnimations(
	USkeleton* Skeleton,
	UObject* Outer,
	const FString& Name,
	//UFbxAnimSequenceImportData* TemplateImportData, 
	//TArray<FbxNode*>& NodeArray
	UDataTable* ReNameTable,
	UMMDExtendAsset* mmdExtend,
	MMD4UE4::VmdMotionInfo* vmdMotionInfo
	)
{
	UAnimSequence* LastCreatedAnim = NULL;



	// we need skeleton to create animsequence
	if (Skeleton == NULL)
	{
		return NULL;
	}

	{
		FString SequenceName = Name;

		//if (ValidTakeCount > 1)
		{
			SequenceName += "_";
			//SequenceName += ANSI_TO_TCHAR(CurAnimStack->GetName());
			SequenceName += Skeleton->GetName();
		}

		// See if this sequence already exists.
		SequenceName = ObjectTools::SanitizeObjectName(SequenceName);

		FString 	ParentPath = FString::Printf(TEXT("%s/%s"), *FPackageName::GetLongPackagePath(*Outer->GetName()), *SequenceName);
		UObject* 	ParentPackage = CreatePackage(NULL, *ParentPath);
		UObject* Object = LoadObject<UObject>(ParentPackage, *SequenceName, NULL, LOAD_None, NULL);
		UAnimSequence * DestSeq = Cast<UAnimSequence>(Object);
		// if object with same name exists, warn user
		if (Object && !DestSeq)
		{
			//AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("Error_AssetExist", "Asset with same name exists. Can't overwrite another asset")), FFbxErrors::Generic_SameNameAssetExists);
			//continue; // Move on to next sequence..
			return LastCreatedAnim;
		}

		// If not, create new one now.
		if (!DestSeq)
		{
			DestSeq = NewObject<UAnimSequence>( ParentPackage, *SequenceName, RF_Public | RF_Standalone);

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(DestSeq);
		}
		else
		{
			DestSeq->RecycleAnimSequence();
		}

		DestSeq->SetSkeleton(Skeleton);
		/*
		// since to know full path, reimport will need to do same
		UFbxAnimSequenceImportData* ImportData = UFbxAnimSequenceImportData::GetImportDataForAnimSequence(DestSeq, TemplateImportData);
		ImportData->SourceFilePath = FReimportManager::SanitizeImportFilename(UFactory::CurrentFilename, DestSeq);
		ImportData->SourceFileTimestamp = IFileManager::Get().GetTimeStamp(*UFactory::CurrentFilename).ToString();
		ImportData->bDirty = false;

		ImportAnimation(Skeleton, DestSeq, Name, SortedLinks, NodeArray, CurAnimStack, ResampleRate, AnimTimeSpan);
		*/
		LastCreatedAnim = DestSeq;
	}
	///////////////////////////////////
	// Create RawCurve -> Track Curve Key
	//////////////////////
	if (LastCreatedAnim)
	{
		/* vmd animation regist*/
		if (!ImportVMDToAnimSequence(LastCreatedAnim, Skeleton, ReNameTable, mmdExtend, vmdMotionInfo))
		{
			//TBD::ERR case
			check(false);
		}
		/* morph animation regist*/
		if (!ImportMorphCurveToAnimSequence(LastCreatedAnim, Skeleton, ReNameTable,vmdMotionInfo))
		{
			//TBD::ERR case
			{
				UE_LOG(LogMMD4UE4_VMDFactory, Error,
					TEXT("ImportMorphCurveToAnimSequence is false root...")
					);
			}
			//check(false);
		}
	}

	/////////////////////////////////////////
	// end process?
	////////////////////////////////////////
	if (LastCreatedAnim)
	{
		/***********************/
		// refresh TrackToskeletonMapIndex
		//LastCreatedAnim->RefreshTrackMapFromAnimTrackNames();
		if (false)
		{
			LastCreatedAnim->BakeTrackCurvesToRawAnimation();
		}
		else
		{
			// otherwise just compress
			LastCreatedAnim->PostProcessSequence();
		}
	}

	return LastCreatedAnim;
}

/******************************************************************************
* Start
* copy from: http://d.hatena.ne.jp/edvakf/touch/20111016/1318716097
* x1~y2 : 0 <= xy <= 1 :bezier points
* x : 0<= x <= 1 : frame rate
******************************************************************************/
float UVmdFactory::interpolateBezier(float x1, float y1, float  x2, float y2, float x) {
	float t = 0.5, s = 0.5;
	for (int i = 0; i < 15; i++) {
		float ft = (3 * s * s * t * x1) + (3 * s * t * t * x2) + (t * t * t) - x;
		if (ft == 0) break; // Math.abs(ft) < 0.00001 でもいいかも
		if (FGenericPlatformMath::Abs(ft) < 0.0001) break;
		if (ft > 0)
			t -= 1.0 / (float)(4 << i);
		else // ft < 0
			t += 1.0 / (float)(4 << i);
		s = 1 - t;
	}
	return (3 * s * s * t * y1) + (3 * s * t * t * y2) + (t * t * t);
}
/** End **********************************************************************/


/*******************
* 既存AnimSequのアセットにVMDの表情データを追加する処理
* MMD4Mecanimuとの総合利用向けテスト機能
**********************/
UAnimSequence * UVmdFactory::AddtionalMorphCurveImportToAnimations(
	UAnimSequence* exsistAnimSequ,
	UDataTable* ReNameTable,
	MMD4UE4::VmdMotionInfo* vmdMotionInfo
	)
{
	USkeleton* Skeleton = NULL;
	// we need skeleton to create animsequence
	if (exsistAnimSequ == NULL)
	{
		return NULL;
	}
	{
		//TDB::if exsite assets need fucn?
		//exsistAnimSequ->RecycleAnimSequence();

		Skeleton = exsistAnimSequ->GetSkeleton();
	}
	///////////////////////////////////
	// Create RawCurve -> Track Curve Key
	//////////////////////
	if (exsistAnimSequ)
	{
		//exsistAnimSequ->NumFrames = vmdMotionInfo->maxFrame;
		//exsistAnimSequ->SequenceLength = FGenericPlatformMath::Max<float>(1.0f / 30.0f*(float)exsistAnimSequ->NumFrames, MINIMUM_ANIMATION_LENGTH);
		/////////////////////////////////
		if (!ImportMorphCurveToAnimSequence(
			exsistAnimSequ,
			Skeleton, 
			ReNameTable, 
			vmdMotionInfo)
			)
		{
			//TBD::ERR case
			check(false);
		}
	}

	/////////////////////////////////////////
	// end process?
	////////////////////////////////////////
	if (exsistAnimSequ)
	{
		bool existAsset = true;
		/***********************/
		// refresh TrackToskeletonMapIndex
		//exsistAnimSequ->RefreshTrackMapFromAnimTrackNames();
		if (existAsset)
		{
			exsistAnimSequ->BakeTrackCurvesToRawAnimation();
		}
		else
		{
			// otherwise just compress
			exsistAnimSequ->PostProcessSequence();
		}
	}

	return exsistAnimSequ;
}
/*******************
* Import Morph Target AnimCurve
* VMDファイルのデータからMorphtargetのFloatCurveをAnimSeqに取り込む
**********************/
bool UVmdFactory::ImportMorphCurveToAnimSequence(
	UAnimSequence* DestSeq,
	USkeleton* Skeleton,
	UDataTable* ReNameTable,
	MMD4UE4::VmdMotionInfo* vmdMotionInfo
	)
{
	if (!DestSeq || !Skeleton || !vmdMotionInfo)
	{
		//TBD:: ERR in Param...
		return false;
	}
	USkeletalMesh * mesh = Skeleton->GetAssetPreviewMesh(DestSeq);// GetPreviewMesh();
	if (!mesh)
	{
		//このルートに入る条件がSkeleton Asset生成後一度もアセットを開いていない場合、
		// NULLの模様。この関数を使うよりも別の手段を考えた方が良さそう…。要調査枠。
		//TDB::ERR.  previewMesh is Null
		{
			UE_LOG(LogMMD4UE4_VMDFactory, Error,
				TEXT("ImportMorphCurveToAnimSequence GetAssetPreviewMesh Not Found...")
				);
		}
		return false;
	}
	/* morph animation regist*/
	for (int i = 0; i < vmdMotionInfo->keyFaceList.Num(); ++i)
	{
		MMD4UE4::VmdFaceTrackList * vmdFaceTrackPtr = &vmdMotionInfo->keyFaceList[i];
		/********************************************/
		//original
		FName Name = *vmdFaceTrackPtr->TrackName;
		if (ReNameTable)
		{
			FName tempUe4Name;
			if (FindTableRowMMD2UEName(ReNameTable,Name,&tempUe4Name) )
			{
				Name = tempUe4Name;
			}
		}
#if 0	/* under ~UE4.10*/
		FSmartNameMapping* NameMapping 
			//= Skeleton->SmartNames.GetContainer(USkeleton::AnimCurveMappingName); 
#else	/* UE4.11~ over */
		const FSmartNameMapping* NameMapping
			//= const_cast<FSmartNameMapping*>(Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName));//UE4.11~
			= Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);//UE4.11~
#endif
		/**********************************/
		//self
		UMorphTarget * morphTargetPtr = mesh->FindMorphTarget(Name);
		if (!morphTargetPtr)
		{
			//TDB::ERR. not found Morph Target(Name) in mesh
			{
				UE_LOG(LogMMD4UE4_VMDFactory, Warning,
					TEXT("ImportMorphCurveToAnimSequence Target Morph Not Found...Search[%s]VMD-Org[%s]"),
					*Name.ToString(), *vmdFaceTrackPtr->TrackName );
			}
			continue;
		}
		/*********************************/
		// Add or retrieve curve
		if (!NameMapping->Exists(Name))
		{
			// mark skeleton dirty
			Skeleton->Modify();
		}

		FSmartName NewName;
		Skeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, Name, NewName);

		// FloatCurve for Morph Target 
		int CurveFlags = ACF_DriveMorphTarget;

		FFloatCurve * CurveToImport
			= static_cast<FFloatCurve *>(DestSeq->RawCurveData.GetCurveData(NewName.UID, FRawCurveTracks::FloatType));
		if (CurveToImport == NULL)
		{
			if (DestSeq->RawCurveData.AddCurveData(NewName, CurveFlags))
			{
				CurveToImport
					= static_cast<FFloatCurve *> (DestSeq->RawCurveData.GetCurveData(NewName.UID, FRawCurveTracks::FloatType));
				CurveToImport->Name = NewName;
			}
			else
			{
				// this should not happen, we already checked before adding
				UE_LOG(LogMMD4UE4_VMDFactory, Warning,
					TEXT("VMD Import: Critical error: no memory?"));
			}
		}
		else
		{
			CurveToImport->FloatCurve.Reset();
			// if existing add these curve flags. 
			CurveToImport->SetCurveTypeFlags(CurveFlags | CurveToImport->GetCurveTypeFlags());
		}
		
		/**********************************************/
		MMD4UE4::VMD_FACE_KEY * faceKeyPtr = NULL;
		for (int s = 0; s < vmdFaceTrackPtr->keyList.Num(); ++s)
		{
			check(vmdFaceTrackPtr->sortIndexList[s] < vmdFaceTrackPtr->keyList.Num());
			faceKeyPtr = &vmdFaceTrackPtr->keyList[vmdFaceTrackPtr->sortIndexList[s]];
			check(faceKeyPtr);
			/********************************************/
			float timeCurve = faceKeyPtr->Frame / 30.0f;
			if (timeCurve > DestSeq->SequenceLength)
			{
				//this key frame(time) more than Target SeqLength ... 
				break;
			}
			CurveToImport->FloatCurve.AddKey(timeCurve, faceKeyPtr->Factor, true);
			/********************************************/
		}

		// update last observed name. If not, sometimes it adds new UID while fixing up that will confuse Compressed Raw Data
		const FSmartNameMapping* Mapping = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
		DestSeq->RawCurveData.RefreshName(Mapping);

		DestSeq->MarkRawDataAsModified();
		/***********************************************************************************/
		// Trace Log ( for debug message , compleat import morph of this track )
		if (true)
		{
			UE_LOG(LogMMD4UE4_VMDFactory, Log,
				TEXT("ImportMorphCurveToAnimSequence Target Morph compleat...NameSearch[%s]VMD-Org[%s], KeyListNum[%d]"),
				*Name.ToString(), *vmdFaceTrackPtr->TrackName, vmdFaceTrackPtr->keyList.Num() );
		}
		/***********************************************************************************/
	}
	return true;
}


/*******************
* Import VMD Animation
* VMDファイルのデータからモーションデータをAnimSeqに取り込む
**********************/
bool UVmdFactory::ImportVMDToAnimSequence(
	UAnimSequence* DestSeq,
	USkeleton* Skeleton,
	UDataTable* ReNameTable,
	UMMDExtendAsset* mmdExtend,
	MMD4UE4::VmdMotionInfo* vmdMotionInfo
	)
{
	// nullptr check in-param
	if (!DestSeq || !Skeleton || !vmdMotionInfo)
	{
		UE_LOG(LogMMD4UE4_VMDFactory, Error,
			TEXT("ImportVMDToAnimSequence : Ref InParam is Null. DestSeq[%x],Skelton[%x],vmdMotionInfo[%x]"),
			DestSeq, Skeleton, vmdMotionInfo );
		//TBD:: ERR in Param...
		return false;
	}
	if (!ReNameTable)
	{
		UE_LOG(LogMMD4UE4_VMDFactory, Warning,
			TEXT("ImportVMDToAnimSequence : Target ReNameTable is null."));
	}
	if (!mmdExtend)
	{
		UE_LOG(LogMMD4UE4_VMDFactory, Warning,
			TEXT("ImportVMDToAnimSequence : Target MMDExtendAsset is null."));
	}
	/********************************/
	DestSeq->NumFrames = vmdMotionInfo->maxFrame;
	DestSeq->SequenceLength = FGenericPlatformMath::Max<float>(1.0f / 30.0f*(float)DestSeq->NumFrames, MINIMUM_ANIMATION_LENGTH);
	/////////////////////////////////
	const int32 NumBones = Skeleton->GetReferenceSkeleton().GetNum();
	DestSeq->RawAnimationData.AddZeroed(NumBones);
	DestSeq->AnimationTrackNames.AddUninitialized(NumBones);


	const TArray<FTransform>& RefBonePose = Skeleton->GetReferenceSkeleton().GetRefBonePose();

	TArray<FRawAnimSequenceTrack> TempRawTrackList;


	check(RefBonePose.Num() == NumBones);
	// SkeletonとのBone関係を登録する＠必須事項
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		DestSeq->AnimationTrackNames[BoneIndex] = Skeleton->GetReferenceSkeleton().GetBoneName(BoneIndex);
		// add mapping to skeleton bone track
		DestSeq->TrackToSkeletonMapTable.Add(FTrackToSkeletonMap(BoneIndex));

		TempRawTrackList.Add(FRawAnimSequenceTrack());
		check(BoneIndex == TempRawTrackList.Num() - 1);
		FRawAnimSequenceTrack& RawTrack = TempRawTrackList[BoneIndex];


		//★TBD:追加処理：以下検討中
		FName targetName = DestSeq->AnimationTrackNames[BoneIndex];
		if (ReNameTable)
		{
			//もし変換テーブルのアセットを指定している場合はテーブルから変換名を取得する
			FMMD2UE4NameTableRow* dataRow;
			FString ContextString;
			dataRow = ReNameTable->FindRow<FMMD2UE4NameTableRow>(targetName, ContextString);
			if (dataRow)
			{
				targetName = FName(*dataRow->MmdOriginalName);
			}
		}
		int vmdKeyListIndex = vmdMotionInfo->FindKeyTrackName(targetName.ToString(), MMD4UE4::VmdMotionInfo::EVMD_KEYBONE);
		if (vmdKeyListIndex == -1)
		{
			{
				UE_LOG(LogMMD4UE4_VMDFactory, Warning,
					TEXT("ImportVMDToAnimSequence Target Bone Not Found...[%s]"),
					*targetName.ToString());
			}
			//nop
			//フレーム分同じ値を設定する
			for (int32 i = 0; i < DestSeq->NumFrames; i++)
			{
				FTransform nrmTrnc;
				nrmTrnc.SetIdentity();
				RawTrack.PosKeys.Add(nrmTrnc.GetTranslation());
				RawTrack.RotKeys.Add(nrmTrnc.GetRotation());
				RawTrack.ScaleKeys.Add(nrmTrnc.GetScale3D());
			}
		}
		else
		{
			check(vmdKeyListIndex > -1);
			int sortIndex = 0;
			int preKeyIndex = -1;
			int nextKeyIndex = vmdMotionInfo->keyBoneList[vmdKeyListIndex].sortIndexList[sortIndex];
			int nextKeyFrame = vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Frame;
			int baseKeyFrame = 0;

			{
				UE_LOG(LogMMD4UE4_VMDFactory, Log,
					TEXT("ImportVMDToAnimSequence Target Bone Found...Name[%s]-KeyNum[%d]"),
					*targetName.ToString(),
					vmdMotionInfo->keyBoneList[vmdKeyListIndex].sortIndexList.Num() );
			}

			//事前に各Trackに対し親Bone抜きにLocal座標で全登録予定のフレーム計算しておく（もっと良い処理があれば…検討）
			//90度以上の軸回転が入るとクォータニオンの為か処理に誤りがあるかで余計な回転が入ってしまう。
			//→上記により、単にZ回転（ターンモーション）で下半身と上半身の軸が物理的にありえない回転の組み合わせになる。バグ。
			for (int32 i = 0; i < DestSeq->NumFrames; i++)
			{
				if (i == 0)
				{
					if (i == nextKeyFrame)
					{
						FTransform tempTranceform(
							FQuat(
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Quaternion[0],
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Quaternion[2] * (-1),
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Quaternion[1],
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Quaternion[3]
							),
							FVector(
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Position[0],
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Position[2] * (-1),
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Position[1]
							)*10.0f,
							FVector(1, 1, 1)
							);
						//リファレンスポーズからKeyのポーズ分移動させた値を初期値とする
						RawTrack.PosKeys.Add(tempTranceform.GetTranslation());
						RawTrack.RotKeys.Add(tempTranceform.GetRotation());
						RawTrack.ScaleKeys.Add(tempTranceform.GetScale3D());

						if (sortIndex + 1 < vmdMotionInfo->keyBoneList[vmdKeyListIndex].sortIndexList.Num())
						{
							sortIndex++;
							preKeyIndex = nextKeyIndex;
							nextKeyIndex = vmdMotionInfo->keyBoneList[vmdKeyListIndex].sortIndexList[sortIndex];
							nextKeyFrame = vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Frame;
						}
					}
					else
					{
						preKeyIndex = nextKeyIndex;
						//例外処理。初期フレーム(0)にKeyが設定されていない
						FTransform nrmTrnc;
						nrmTrnc.SetIdentity();
						RawTrack.PosKeys.Add(nrmTrnc.GetTranslation());
						RawTrack.RotKeys.Add(nrmTrnc.GetRotation());
						RawTrack.ScaleKeys.Add(nrmTrnc.GetScale3D());
					}
				}
				else
				{
					float blendRate = 1;
					FTransform NextTranc;
					FTransform PreTranc;
					FTransform NowTranc;

					NextTranc.SetIdentity();
					PreTranc.SetIdentity();
					NowTranc.SetIdentity();

					if (nextKeyIndex > 0)
					{
						MMD4UE4::VMD_KEY & PreKey = vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[preKeyIndex];
						MMD4UE4::VMD_KEY & NextKey = vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex];
						if (NextKey.Frame <= (uint32)i)
						{
							blendRate = 1.0f;
						}
						else
						{ 
							//TBD::フレーム間が1だと0.5で計算されない?
							blendRate = 1.0f - (float)(NextKey.Frame - (uint32)i) / (float)(NextKey.Frame - PreKey.Frame);
						}
						//pose
						NextTranc.SetLocation(
							FVector(
							NextKey.Position[0],
							NextKey.Position[2] * (-1),
							NextKey.Position[1]
							));
						PreTranc.SetLocation(
							FVector(
							PreKey.Position[0],
							PreKey.Position[2] * (-1),
							PreKey.Position[1]
							));

						NowTranc.SetLocation(
							FVector(
							interpolateBezier(
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_X] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_X] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_X] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_X] / 127.0f,
							blendRate
							)*(NextTranc.GetTranslation().X - PreTranc.GetTranslation().X) + PreTranc.GetTranslation().X
							,
							interpolateBezier(
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_Z] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_Z] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_Z] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_Z] / 127.0f,
							blendRate
							)*(NextTranc.GetTranslation().Y - PreTranc.GetTranslation().Y) + PreTranc.GetTranslation().Y
							,
							interpolateBezier(
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_Y] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_Y] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_Y] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_Y] / 127.0f,
							blendRate
							)*(NextTranc.GetTranslation().Z - PreTranc.GetTranslation().Z) + PreTranc.GetTranslation().Z
							)
							);
						//rot
						NextTranc.SetRotation(
							FQuat(
							NextKey.Quaternion[0],
							NextKey.Quaternion[2] * (-1),
							NextKey.Quaternion[1],
							NextKey.Quaternion[3]
							));
						PreTranc.SetRotation(
							FQuat(
							PreKey.Quaternion[0],
							PreKey.Quaternion[2] * (-1),
							PreKey.Quaternion[1],
							PreKey.Quaternion[3]
							));
#if 0
						float tempBezR[4];
						NowTranc.SetRotation(
							FQuat(
							tempBezR[0] = interpolateBezier(
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							blendRate
							)*(NextTranc.GetRotation().X - PreTranc.GetRotation().X) + PreTranc.GetRotation().X
							,
							tempBezR[1] = interpolateBezier(
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							blendRate
							)*(NextTranc.GetRotation().Y - PreTranc.GetRotation().Y) + PreTranc.GetRotation().Y
							,
							tempBezR[2] = interpolateBezier(
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							blendRate
							)*(NextTranc.GetRotation().Z - PreTranc.GetRotation().Z) + PreTranc.GetRotation().Z
							,
							tempBezR[3] = interpolateBezier(
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							blendRate
							)*(NextTranc.GetRotation().W - PreTranc.GetRotation().W) + PreTranc.GetRotation().W
							)
							);
						UE_LOG(LogMMD4UE4_VMDFactory, Warning,
							TEXT("interpolateBezier Rot:[%s],F[%d/%d],BLD[%.2f],BEZ[%.3f][%.3f][%.3f][%.3f]"),
							*targetName.ToString(), i, NextKey.Frame, blendRate, tempBezR[0], tempBezR[1], tempBezR[2], tempBezR[3]
							);
#else
						float bezirT = interpolateBezier(
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_0][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_X][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							NextKey.Bezier[D_VMD_KEY_BEZIER_AR_0_BEZ_1][D_VMD_KEY_BEZIER_AR_1_BEZ_Y][D_VMD_KEY_BEZIER_AR_2_KND_R] / 127.0f,
							blendRate
							);
						NowTranc.SetRotation(
							FQuat::Slerp(PreTranc.GetRotation(), NextTranc.GetRotation(), bezirT)
							);
						/*UE_LOG(LogMMD4UE4_VMDFactory, Warning,
							TEXT("interpolateBezier Rot:[%s],F[%d/%d],BLD[%.2f],biz[%.2f]BEZ[%s]"),
							*targetName.ToString(), i, NextKey.Frame, blendRate, bezirT,*NowTranc.GetRotation().ToString()
							);*/
#endif
					}
					else
					{
						NowTranc.SetLocation(
							FVector(
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Position[0],
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Position[2] * (-1),
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Position[1]
							));
						NowTranc.SetRotation(
							FQuat(
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Quaternion[0],
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Quaternion[2] * (-1),
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Quaternion[1],
							vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Quaternion[3]
							));
						//TBD:このルートが存在する模様、処理の再検討が必要
						//check(false);
					}

					FTransform tempTranceform(
						NowTranc.GetRotation(),
						NowTranc.GetTranslation()*10.0f,
						FVector(1, 1, 1)
						);
					//リファレンスポーズからKeyのポーズ分移動させた値を初期値とする
					RawTrack.PosKeys.Add(tempTranceform.GetTranslation());
					RawTrack.RotKeys.Add(tempTranceform.GetRotation());
					RawTrack.ScaleKeys.Add(tempTranceform.GetScale3D());
					if (nextKeyFrame == i)
					{
						//該当キーと一致。直値代入
						//RawTrack.PosKeys.Add()
						if (sortIndex + 1 < vmdMotionInfo->keyBoneList[vmdKeyListIndex].sortIndexList.Num())
						{
							sortIndex++;
							preKeyIndex = nextKeyIndex;
							nextKeyIndex = vmdMotionInfo->keyBoneList[vmdKeyListIndex].sortIndexList[sortIndex];
							nextKeyFrame = vmdMotionInfo->keyBoneList[vmdKeyListIndex].keyList[nextKeyIndex].Frame;
						}
						else
						{
							//TDB::Last Frame, not ++
						}
					}
				}
			}
		}
	}
	////////////////////////////////////
	//親ボーンの順番::TBD(IK用)
	TArray<int> TempBoneTreeRefSkel;
	int tempParentID = -1;
	while (TempBoneTreeRefSkel.Num() == NumBones)
	{
		for (int i = 0; i < NumBones; ++i)
		{
			//if (tempParentID == )
			{
				TempBoneTreeRefSkel.Add(i);
			}
		}
	}

	//全フレームに対し事前に各ボーンの座標を計算しRawTrackに追加する＠必須
	//TBD::多分バグがあるはず…
	FTransform subLocRef;
	FTransform tempLoc;
	GWarn->BeginSlowTask(LOCTEXT("BeginImportAnimation", "Importing Animation"), true);
	for (int32 k = 0; k < DestSeq->NumFrames; k++)
	{
		// update status
		FFormatNamedArguments Args;
		//Args.Add(TEXT("TrackName"), FText::FromName(BoneName));
		Args.Add(TEXT("NowKey"), FText::AsNumber(k));
		Args.Add(TEXT("TotalKey"), FText::AsNumber(DestSeq->NumFrames));
		//Args.Add(TEXT("TrackIndex"), FText::AsNumber(SourceTrackIdx + 1));
		Args.Add(TEXT("TotalTracks"), FText::AsNumber(NumBones));
		//const FText StatusUpate = FText::Format(LOCTEXT("ImportingAnimTrackDetail", "Importing Animation Track [{TrackName}] ({TrackIndex}/{TotalTracks}) - TotalKey {TotalKey}"), Args);
		const FText StatusUpate
			= FText::Format(LOCTEXT("ImportingAnimTrackDetail",
				"Importing Animation Track Key({NowKey}/{TotalKey}) - TotalTracks {TotalTracks}"),
				Args);
		GWarn->StatusForceUpdate(k, DestSeq->NumFrames, StatusUpate);

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			DestSeq->AnimationTrackNames[BoneIndex] = Skeleton->GetReferenceSkeleton().GetBoneName(BoneIndex);

			FRawAnimSequenceTrack& RawTrack = DestSeq->RawAnimationData[BoneIndex];

			FRawAnimSequenceTrack& LocalRawTrack = TempRawTrackList[BoneIndex];

			/////////////////////////////////////
			///お試し、親ボーンの順番を考えない
			{
				//移動後の位置 = 元の位置 + 平行移動
				//移動後の回転 = 回転
				RawTrack.PosKeys.Add(LocalRawTrack.PosKeys[k] + RefBonePose[BoneIndex].GetTranslation());
				RawTrack.RotKeys.Add(LocalRawTrack.RotKeys[k]);
				RawTrack.ScaleKeys.Add(LocalRawTrack.ScaleKeys[k]);
			}
		}
	}
	if(mmdExtend)
	{
		FName targetName;
		// ik Target loop int setup ...
		for (int32 ikTargetIndex = 0; ikTargetIndex < mmdExtend->IkInfoList.Num(); ++ikTargetIndex)
		{
			//check bone has skeleton .
			mmdExtend->IkInfoList[ikTargetIndex].checkIKIndex = true;
			//vmd
			//★TBD:追加処理：以下検討中
			targetName = mmdExtend->IkInfoList[ikTargetIndex].IKBoneName;
			if (ReNameTable)
			{
				//もし変換テーブルのアセットを指定している場合はテーブルから変換名を取得する
				FMMD2UE4NameTableRow* dataRow;
				FString ContextString;
				dataRow = ReNameTable->FindRow<FMMD2UE4NameTableRow>(targetName, ContextString);
				if (dataRow)
				{
					targetName = FName(*dataRow->MmdOriginalName);
				}
			}
			if ((mmdExtend->IkInfoList[ikTargetIndex].IKBoneIndexVMDKey
				= vmdMotionInfo->FindKeyTrackName(targetName.ToString(), MMD4UE4::VmdMotionInfo::EVMD_KEYBONE)) == -1)
			{
				mmdExtend->IkInfoList[ikTargetIndex].checkIKIndex = false;
				UE_LOG(LogMMD4UE4_VMDFactory, Warning,
					TEXT("IKBoneIndexVMDKey index (skelton) not found...name[%s]"),
					*targetName.ToString()
					);
				//not found ik key has this vmd data.
				continue; 
			}

			//search skeleton bone index
			//ik bone
			if ((mmdExtend->IkInfoList[ikTargetIndex].IKBoneIndex
				= FindRefBoneInfoIndexFromBoneName(Skeleton->GetReferenceSkeleton(), targetName)) == -1)
			{
				mmdExtend->IkInfoList[ikTargetIndex].checkIKIndex = false;
				UE_LOG(LogMMD4UE4_VMDFactory, Warning,
					TEXT("IKBoneIndex index (skelton) not found...name[%s]"),
					*targetName.ToString()
					);
				continue;
			}
			//ik target bone
			targetName = mmdExtend->IkInfoList[ikTargetIndex].TargetBoneName;
			if (ReNameTable)
			{
				//もし変換テーブルのアセットを指定している場合はテーブルから変換名を取得する
				FMMD2UE4NameTableRow* dataRow;
				FString ContextString;
				dataRow = ReNameTable->FindRow<FMMD2UE4NameTableRow>(targetName, ContextString);
				if (dataRow)
				{
					targetName = FName(*dataRow->MmdOriginalName);
				}
			}
			if ((mmdExtend->IkInfoList[ikTargetIndex].TargetBoneIndex
				= FindRefBoneInfoIndexFromBoneName(Skeleton->GetReferenceSkeleton(), targetName)) == -1)
			{
				mmdExtend->IkInfoList[ikTargetIndex].checkIKIndex = false;
				UE_LOG(LogMMD4UE4_VMDFactory, Warning,
					TEXT("TargetBoneIndex index (skelton) not found...name[%s]"),
					*targetName.ToString()
					);
				continue;
			}
			//loop sub bone
			for (int32 subBone = 0; subBone < mmdExtend->IkInfoList[ikTargetIndex].ikLinkList.Num(); ++subBone)
			{

				//ik target bone
				targetName = mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[subBone].BoneName;
				if (ReNameTable)
				{
					//もし変換テーブルのアセットを指定している場合はテーブルから変換名を取得する
					FMMD2UE4NameTableRow* dataRow;
					FString ContextString;
					dataRow = ReNameTable->FindRow<FMMD2UE4NameTableRow>(targetName, ContextString);
					if (dataRow)
					{
						targetName = FName(*dataRow->MmdOriginalName);
					}
				}
				if((mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[subBone].BoneIndex
					= FindRefBoneInfoIndexFromBoneName(Skeleton->GetReferenceSkeleton(), targetName)) == -1)
				{
					mmdExtend->IkInfoList[ikTargetIndex].checkIKIndex = false;
					UE_LOG(LogMMD4UE4_VMDFactory, Warning,
						TEXT("sub BoneIndex index (skelton) not found...name[%s],sub[%d]"),
						*targetName.ToString(),
						subBone
						);
					continue;
				}
			}
		}
		//TBD::ここにCCD-IK再計算処理
		//案1：各フレーム計算中に逐次処理
		//案2：FKを全フレーム計算完了後にIKだけまとめてフレーム単位で再計算

		//
		for (int32 k = 0; k < DestSeq->NumFrames; k++)
		{
			// ik Target loop func...
			for (int32 ikTargetIndex = 0; ikTargetIndex < mmdExtend->IkInfoList.Num(); ++ikTargetIndex)
			{
				//check 
				if (!mmdExtend->IkInfoList[ikTargetIndex].checkIKIndex)
				{
					/*UE_LOG(LogMMD4UE4_VMDFactory, Warning,
						TEXT("IK func skip: Parameter is insufficient.[%s]..."),
						*mmdExtend->IkInfoList[ikTargetIndex].IKBoneName.ToString()
						);*/
					continue;
				}
				int32 loopMax = mmdExtend->IkInfoList[ikTargetIndex].LoopNum;
				int32 ilLinklistNum = mmdExtend->IkInfoList[ikTargetIndex].ikLinkList.Num();
				//TBD::もっと良い方があれば検討(Loc→Glbに再計算している為、計算コストが高すぎると推測)
				//ここで事前にIK対象のGlb座標等を計算しておく
				FTransform tempGlbIkBoneTrsf;
				FTransform tempGlbTargetBoneTrsf;
				TArray<FTransform> tempGlbIkLinkTrsfList;
				//get glb trsf
				/*DestSeq->RawAnimationData[mmdExtend->IkInfoList[ikTargetIndex].IKBoneIndex].PosKeys[k]
					= RefBonePose[mmdExtend->IkInfoList[ikTargetIndex].IKBoneIndex].GetTranslation();
				DestSeq->RawAnimationData[mmdExtend->IkInfoList[ikTargetIndex].IKBoneIndex].RotKeys[k]
					= RefBonePose[mmdExtend->IkInfoList[ikTargetIndex].IKBoneIndex].GetRotation();
				DestSeq->RawAnimationData[mmdExtend->IkInfoList[ikTargetIndex].IKBoneIndex].ScaleKeys[k]
					= FVector(1);*/
				tempGlbIkBoneTrsf = CalcGlbTransformFromBoneIndex(
					DestSeq,
					Skeleton,
					mmdExtend->IkInfoList[ikTargetIndex].IKBoneIndex,
					k
					);
				/*DestSeq->RawAnimationData[mmdExtend->IkInfoList[ikTargetIndex].TargetBoneIndex].PosKeys[k]
					= RefBonePose[mmdExtend->IkInfoList[ikTargetIndex].TargetBoneIndex].GetTranslation();
				DestSeq->RawAnimationData[mmdExtend->IkInfoList[ikTargetIndex].TargetBoneIndex].RotKeys[k]
					= RefBonePose[mmdExtend->IkInfoList[ikTargetIndex].TargetBoneIndex].GetRotation();
				DestSeq->RawAnimationData[mmdExtend->IkInfoList[ikTargetIndex].TargetBoneIndex].ScaleKeys[k]
					= FVector(1);*/
				tempGlbTargetBoneTrsf = CalcGlbTransformFromBoneIndex(
					DestSeq,
					Skeleton,
					mmdExtend->IkInfoList[ikTargetIndex].TargetBoneIndex,
					k
					);
				tempGlbIkLinkTrsfList.AddZeroed(ilLinklistNum);
				for (int32 glbIndx = 0; glbIndx < ilLinklistNum; ++glbIndx)
				{
					/*DestSeq->RawAnimationData[mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[glbIndx].BoneIndex].PosKeys[k]
						= RefBonePose[mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[glbIndx].BoneIndex].GetTranslation();
					DestSeq->RawAnimationData[mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[glbIndx].BoneIndex].RotKeys[k]
						= RefBonePose[mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[glbIndx].BoneIndex].GetRotation();
					DestSeq->RawAnimationData[mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[glbIndx].BoneIndex].ScaleKeys[k]
						= FVector(1);*/
					tempGlbIkLinkTrsfList[glbIndx] = CalcGlbTransformFromBoneIndex(
						DestSeq,
						Skeleton,
						mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[glbIndx].BoneIndex,
						k
						);
				}
				//execute 
				//
				FTransform tempCalcIKTrns;
				tempCalcIKTrns.SetIdentity();
				//
				FVector vecToIKTargetPose;
				FVector vecToIKLinkPose;
				FVector asix;
				FQuat	qt;
				float	angle = 0;
				int32	rawIndex = 0;
				int32	ikt = loopMax / 2;
				for (int32 loopCount = 0; loopCount < loopMax; ++loopCount)
				{
					for (int32 linkBoneIndex = 0; linkBoneIndex < ilLinklistNum; ++linkBoneIndex)
					{
						rawIndex = mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[linkBoneIndex].BoneIndex;
#if 0 //test ik bug
						////////////////////
						// Ref: http://pr0jectze10.blogspot.jp/2011/06/ik-part2.html
						////////////////////
						// IKターゲットボーンまでのベクトル
						vecToIKTargetPose
							= tempGlbTargetBoneTrsf.GetTranslation() - tempGlbIkLinkTrsfList[linkBoneIndex].GetTranslation();
						// IKボーンまでのベクトル 
						vecToIKLinkPose
							= tempGlbIkBoneTrsf.GetTranslation() - tempGlbIkLinkTrsfList[linkBoneIndex].GetTranslation();

						// 2つのベクトルの外積（上軸）を算出 
						asix = FVector::CrossProduct(vecToIKTargetPose, vecToIKLinkPose);
						// 軸の数値が正常で、2つのベクトルの向きが不一致の場合（ベクトルの向きが同じ場合、外積は0になる）
						if (asix.SizeSquared()>0)
						{
							// 法線化
							asix.Normalize();
							vecToIKTargetPose.Normalize();
							vecToIKLinkPose.Normalize();
							// 2つのベクトルの内積を計算して、ベクトル間のラジアン角度を算出 
							angle = FMath::Acos(FMath::Clamp<float>( FVector::DotProduct(vecToIKTargetPose, vecToIKLinkPose),-1,1));
							//angle = FVector::DotProduct(vecToIKTargetPose, vecToIKLinkPose);
							float RotLimitRad = FMath::DegreesToRadians(mmdExtend->IkInfoList[ikTargetIndex].RotLimit);
							if (angle > RotLimitRad)
							{
								angle = RotLimitRad;
							}
							// 任意軸回転クォータニオンを作成  
							qt = FQuat(asix, angle);
							// 変換行列に合成 
							tempCalcIKTrns.SetIdentity();
							//tempCalcIKTrns.SetTranslation(DestSeq->RawAnimationData[rawIndex].PosKeys[k]);
							tempCalcIKTrns.SetRotation(DestSeq->RawAnimationData[rawIndex].RotKeys[k]);
							tempCalcIKTrns *= FTransform(qt);

							//軸制限計算
							if (mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[linkBoneIndex].RotLockFlag == 1)
							{
								FVector eulerVec = tempCalcIKTrns.GetRotation().Euler();
								FVector subEulerAngleVec = eulerVec;

								subEulerAngleVec = ClampVector(
									subEulerAngleVec,
									mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[linkBoneIndex].RotLockMin,
									mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[linkBoneIndex].RotLockMax
									);
								subEulerAngleVec -= eulerVec;
								//回転軸制限補正
								tempCalcIKTrns *= FTransform(FQuat::MakeFromEuler(subEulerAngleVec));
							}
							//CCD-IK後の回転軸更新
							DestSeq->RawAnimationData[rawIndex].RotKeys[k]
								= tempCalcIKTrns.GetRotation();
#if 0
							UE_LOG(LogMMD4UE4_VMDFactory, Warning,
								TEXT("test.[%d][%s][%lf/%lf]..."),
								k,
								*mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[linkBoneIndex].BoneName.ToString(),
								angle,
								angle*180/3.14
								);
#endif
							// recalc glb trans
							tempGlbIkBoneTrsf = CalcGlbTransformFromBoneIndex(
								DestSeq,
								Skeleton,
								mmdExtend->IkInfoList[ikTargetIndex].IKBoneIndex,
								k
								);
							tempGlbTargetBoneTrsf = CalcGlbTransformFromBoneIndex(
								DestSeq,
								Skeleton,
								mmdExtend->IkInfoList[ikTargetIndex].TargetBoneIndex,
								k
								);
							tempGlbIkLinkTrsfList.AddZeroed(ilLinklistNum);
							for (int32 glbIndx = 0; glbIndx < ilLinklistNum; ++glbIndx)
							{
								tempGlbIkLinkTrsfList[glbIndx] = CalcGlbTransformFromBoneIndex(
									DestSeq,
									Skeleton,
									mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[glbIndx].BoneIndex,
									k
									);
							}
							/////////////
						}
#else
						//////////////////////////////////////////////////////////
						// test
						//from dxlib pmx ik, kai
						//
						//and
						//ref: http://marupeke296.com/DXG_No20_TurnUsingQuaternion.html
						// j = linkBoneIndex
						//////////////////////////////
						FMMD_IKLINK * IKBaseLinkPtr = &mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[linkBoneIndex];
						// IKターゲットボーンまでのベクトル
						vecToIKTargetPose = tempGlbIkLinkTrsfList[linkBoneIndex].GetTranslation();
						vecToIKTargetPose -= tempGlbTargetBoneTrsf.GetTranslation();
						// IKボーンまでのベクトル 
						vecToIKLinkPose = tempGlbIkLinkTrsfList[linkBoneIndex].GetTranslation();
						vecToIKLinkPose -= tempGlbIkBoneTrsf.GetTranslation();
						// 法線化
						vecToIKTargetPose.Normalize();
						vecToIKLinkPose.Normalize();

						FVector v1, v2;
						v1 = vecToIKTargetPose;
						v2 = vecToIKLinkPose;
						if ((v1.X - v2.X) * (v1.X - v2.X)
							+ (v1.Y - v2.Y) * (v1.Y - v2.Y)
							+ (v1.Z - v2.Z) * (v1.Z - v2.Z) 
							< 0.0000001f) break;

						FVector v;
						v = FVector::CrossProduct(v1, v2);
						// calculate roll axis
						//親計算：無駄計算を省きたいがうまい方法が見つからないので保留
						FVector ChainParentBone_Asix;
						FTransform tempGlbChainParentBoneTrsf = CalcGlbTransformFromBoneIndex(
							DestSeq,
							Skeleton,
							Skeleton->GetReferenceSkeleton().GetParentIndex(IKBaseLinkPtr->BoneIndex),//parent
							k
							);
						if (IKBaseLinkPtr->RotLockFlag == 1 && loopCount < ikt)
						{
							if (IKBaseLinkPtr->RotLockMin.Y == 0 && IKBaseLinkPtr->RotLockMax.Y == 0
								&& IKBaseLinkPtr->RotLockMin.Z == 0 && IKBaseLinkPtr->RotLockMax.Z == 0)
							{
								ChainParentBone_Asix = tempGlbChainParentBoneTrsf.GetUnitAxis(EAxis::X);
								float vvx = FVector::DotProduct(v, ChainParentBone_Asix);
								v.X = vvx >= 0.0f ? 1.0f : -1.0f;
								v.Y = 0.0f;
								v.Z = 0.0f;
							}
							else if (IKBaseLinkPtr->RotLockMin.X == 0 && IKBaseLinkPtr->RotLockMax.X == 0
								&& IKBaseLinkPtr->RotLockMin.Z == 0 && IKBaseLinkPtr->RotLockMax.Z == 0)
							{
								ChainParentBone_Asix = tempGlbChainParentBoneTrsf.GetUnitAxis(EAxis::Y);
								float vvy = FVector::DotProduct(v, ChainParentBone_Asix);
								v.Y = vvy >= 0.0f ? 1.0f : -1.0f;
								v.X = 0.0f;
								v.Z = 0.0f;
							}
							else if (IKBaseLinkPtr->RotLockMin.X == 0 && IKBaseLinkPtr->RotLockMax.X == 0
								&& IKBaseLinkPtr->RotLockMin.Y == 0 && IKBaseLinkPtr->RotLockMax.Y == 0)
							{
								ChainParentBone_Asix = tempGlbChainParentBoneTrsf.GetUnitAxis(EAxis::Z);
								float vvz = FVector::DotProduct(v, ChainParentBone_Asix);
								v.Z = vvz >= 0.0f ? 1.0f : -1.0f;
								v.X = 0.0f;
								v.Y = 0.0f;
							}
							else
							{
								// calculate roll axis
								FVector vv;

								ChainParentBone_Asix = tempGlbChainParentBoneTrsf.GetUnitAxis(EAxis::X);
								vv.X = FVector::DotProduct(v, ChainParentBone_Asix);
								ChainParentBone_Asix = tempGlbChainParentBoneTrsf.GetUnitAxis(EAxis::Y);
								vv.Y = FVector::DotProduct(v, ChainParentBone_Asix);
								ChainParentBone_Asix = tempGlbChainParentBoneTrsf.GetUnitAxis(EAxis::Z);
								vv.Z = FVector::DotProduct(v, ChainParentBone_Asix);

								v = vv;
								v.Normalize();
							}
						}
						else
						{
							// calculate roll axis
							FVector vv;

							ChainParentBone_Asix = tempGlbChainParentBoneTrsf.GetUnitAxis(EAxis::X);
							vv.X = FVector::DotProduct(v, ChainParentBone_Asix);
							ChainParentBone_Asix = tempGlbChainParentBoneTrsf.GetUnitAxis(EAxis::Y);
							vv.Y = FVector::DotProduct(v, ChainParentBone_Asix);
							ChainParentBone_Asix = tempGlbChainParentBoneTrsf.GetUnitAxis(EAxis::Z);
							vv.Z = FVector::DotProduct(v, ChainParentBone_Asix);

							v = vv;
							v.Normalize();
						}


						// calculate roll angle of [k]th bone(limited by p_IK[i].dlimit*(k+1)*2)
						float Cos = FVector::DotProduct(v1,v2);
						if (Cos >  1.0f) Cos = 1.0f;
						if (Cos < -1.0f) Cos = -1.0f;

						float Rot = 0.5f * FMath::Acos(Cos);
						float RotLimitRad = FMath::DegreesToRadians(mmdExtend->IkInfoList[ikTargetIndex].RotLimit);
						FVector RotLockMinRad = IKBaseLinkPtr->RotLockMin * FMath::DegreesToRadians(1);
						FVector RotLockMaxRad = IKBaseLinkPtr->RotLockMax * FMath::DegreesToRadians(1);
						//TBD::単位角度の制限確認(ただし、処理があっているか不明…）
						if (Rot > RotLimitRad * (linkBoneIndex + 1) * 2)
						{
							Rot = RotLimitRad * (linkBoneIndex + 1) * 2;
						}
						/* 暫定：Dxlib版
						float IKsin, IKcos;
						IKsin = FMath::Sin(Rot);
						IKcos = FMath::Cos(Rot);
						FQuat qIK(
							v.X * IKsin,
							v.Y * IKsin,
							v.Z * IKsin,
							IKcos
							);	*/
						//UE4版：軸と角度からQuar作成
						FQuat qIK(v, Rot);

						//chainBone ik quatがGlbかLocかIKのみか不明。・・暫定
						FQuat ChainBone_IKQuat = tempGlbIkBoneTrsf.GetRotation();
						ChainBone_IKQuat = qIK * ChainBone_IKQuat ;
						tempGlbIkBoneTrsf.SetRotation(ChainBone_IKQuat);

						FMatrix ChainBone_IKmat;
						ChainBone_IKmat = tempGlbIkBoneTrsf.ToMatrixNoScale(); //時たまここでクラッシュする・・・解決策を探す。

						//軸制限計算
						if (IKBaseLinkPtr->RotLockFlag == 1)
						{
							// 軸回転角度を算出
							if ((RotLockMinRad.X > -1.570796f) & (RotLockMaxRad.X < 1.570796f))
							{
								// Z*X*Y順
								// X軸回り
								float fLimit = 1.535889f;			// 88.0f/180.0f*3.14159265f;
								float fSX = -ChainBone_IKmat.M[2][1];				// sin(θx)
								float fX = FMath::Asin(fSX);			// X軸回り決定
								float fCX = FMath::Cos(fX);

								// ジンバルロック回避
								if (FMath::Abs<float>(fX) > fLimit)
								{
									fX = (fX < 0) ? -fLimit : fLimit; 
									fCX = FMath::Cos(fX);
								}

								// Y軸回り
								float fSY = ChainBone_IKmat.M[2][0] / fCX;
								float fCY = ChainBone_IKmat.M[2][2] / fCX;
								float fY = FMath::Atan2(fSY, fSX);	// Y軸回り決定

								// Z軸回り
								float fSZ = ChainBone_IKmat.M[0][1] / fCX;
								float fCZ = ChainBone_IKmat.M[1][1] / fCX;
								float fZ = FMath::Atan2(fSZ, fCZ);

								// 角度の制限
								FVector fixRotAngleVec(fX,fY,fZ);
								CheckLimitAngle(
									RotLockMinRad,
									RotLockMaxRad,
									&fixRotAngleVec, 
									(linkBoneIndex < ikt)
									);
								// 決定した角度でベクトルを回転
								FMatrix mX, mY, mZ, mT;

								CreateRotationXMatrix(&mX, fixRotAngleVec.X);
								CreateRotationXMatrix(&mY, fixRotAngleVec.Y);
								CreateRotationXMatrix(&mZ, fixRotAngleVec.Z);

								MV1LoadModelToVMD_CreateMultiplyMatrixRotOnly(&mT, &mZ, &mX);
								MV1LoadModelToVMD_CreateMultiplyMatrixRotOnly(&ChainBone_IKmat, &mT, &mY);
							}
							else if ((RotLockMinRad.X > -1.570796f) & (RotLockMaxRad.X < 1.570796f))
							{
								// X*Y*Z順
								// Y軸回り
								float fLimit = 1.535889f;		// 88.0f/180.0f*3.14159265f;
								float fSY = -ChainBone_IKmat.M[0][2];			// sin(θy)
								float fY = FMath::Asin(fSY);	// Y軸回り決定
								float fCY = FMath::Cos(fY);

								// ジンバルロック回避
								if (FMath::Abs<float>(fY) > fLimit)
								{
									fY = (fY < 0) ? -fLimit : fLimit;
									fCY = FMath::Cos(fY);
								}

								// X軸回り
								float fSX = ChainBone_IKmat.M[1][2] / fCY;
								float fCX = ChainBone_IKmat.M[2][2] / fCY;
								float fX = FMath::Atan2(fSX, fCX);	// X軸回り決定

								// Z軸回り
								float fSZ = ChainBone_IKmat.M[0][1] / fCY;
								float fCZ = ChainBone_IKmat.M[0][0] / fCY;
								float fZ = FMath::Atan2(fSZ, fCZ);	// Z軸回り決定

								// 角度の制限
								FVector fixRotAngleVec(fX, fY, fZ);
								CheckLimitAngle(
									RotLockMinRad,
									RotLockMaxRad,
									&fixRotAngleVec,
									(linkBoneIndex < ikt)
									);

								// 決定した角度でベクトルを回転
								FMatrix mX, mY, mZ, mT;

								CreateRotationXMatrix(&mX, fixRotAngleVec.X);
								CreateRotationXMatrix(&mY, fixRotAngleVec.Y);
								CreateRotationXMatrix(&mZ, fixRotAngleVec.Z);

								MV1LoadModelToVMD_CreateMultiplyMatrixRotOnly(&mT, &mX, &mY);
								MV1LoadModelToVMD_CreateMultiplyMatrixRotOnly(&ChainBone_IKmat, &mT, &mZ);
							}
							else
							{
								// Y*Z*X順
								// Z軸回り
								float fLimit = 1.535889f;		// 88.0f/180.0f*3.14159265f;
								float fSZ = -ChainBone_IKmat.M[1][0];			// sin(θy)
								float fZ = FMath::Asin(fSZ);	// Y軸回り決定
								float fCZ = FMath::Cos(fZ);

								// ジンバルロック回避
								if (FMath::Abs(fZ) > fLimit)
								{
									fZ = (fZ < 0) ? -fLimit : fLimit;
									fCZ = FMath::Cos(fZ);
								}

								// X軸回り
								float fSX = ChainBone_IKmat.M[1][2] / fCZ;
								float fCX = ChainBone_IKmat.M[1][1] / fCZ;
								float fX = FMath::Atan2(fSX, fCX);	// X軸回り決定

								// Y軸回り
								float fSY = ChainBone_IKmat.M[2][0] / fCZ;
								float fCY = ChainBone_IKmat.M[0][0] / fCZ;
								float fY = FMath::Atan2(fSY, fCY);	// Z軸回り決定
								
								// 角度の制限
								FVector fixRotAngleVec(fX, fY, fZ);
								CheckLimitAngle(
									RotLockMinRad,
									RotLockMaxRad,
									&fixRotAngleVec,
									(linkBoneIndex < ikt)
									);

								// 決定した角度でベクトルを回転
								FMatrix mX, mY, mZ, mT;

								CreateRotationXMatrix(&mX, fixRotAngleVec.X);
								CreateRotationXMatrix(&mY, fixRotAngleVec.Y);
								CreateRotationXMatrix(&mZ, fixRotAngleVec.Z);

								MV1LoadModelToVMD_CreateMultiplyMatrixRotOnly(&mT, &mY, &mZ);
								MV1LoadModelToVMD_CreateMultiplyMatrixRotOnly(&ChainBone_IKmat, &mT, &mX);
							}
							//QuatConvertFromMatrix(ChainBone->IKQuat, ChainBone->IKmat);
							tempGlbIkBoneTrsf.SetFromMatrix(ChainBone_IKmat);
							tempGlbIkBoneTrsf.SetScale3D(FVector(1));//reset scale
							DestSeq->RawAnimationData[rawIndex].RotKeys[k]
								= tempGlbIkBoneTrsf.GetRotation();
#if 0
							UE_LOG(LogMMD4UE4_VMDFactory, Warning,
								TEXT("test.Loop[%d]LinkIndx[%d]Ke[%d]Nam[%s]Trf[%s]..."),
								loopCount,
								linkBoneIndex,
								k,
								*mmdExtend->IkInfoList[ikTargetIndex].ikLinkList[linkBoneIndex].BoneName.ToString(),
								*tempGlbIkBoneTrsf.ToString()
								);
							UE_LOG(LogMMD4UE4_VMDFactory, Error,
								TEXT("ikmat[%s]..."),
								*ChainBone_IKmat.ToString()
								);
#endif
						}
						else
						{
							//non limit
							DestSeq->RawAnimationData[rawIndex].RotKeys[k]
								= tempGlbIkBoneTrsf.GetRotation();
						}
						//////////////////////////////////////////////////
#endif
					}
				}
			}
		}
	}
	GWarn->EndSlowTask();
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////
// Ｘ軸を中心とした回転行列を作成する
void CreateRotationXMatrix(FMatrix *Out, float Angle)
{
	float Sin, Cos;

	//_SINCOS(Angle, &Sin, &Cos);
	//	Sin = sinf( Angle ) ;
	//	Cos = cosf( Angle ) ;
	Sin = FMath::Sin(Angle);
	Cos = FMath::Cos(Angle);

	//_MEMSET(Out, 0, sizeof(MATRIX));
	FMemory::Memzero(Out,sizeof(FMatrix));
	Out->M[0][0] = 1.0f;
	Out->M[1][1] = Cos;
	Out->M[1][2] = Sin;
	Out->M[2][1] = -Sin;
	Out->M[2][2] = Cos;
	Out->M[3][3] = 1.0f;

	//return 0;
}
// 回転成分だけの行列の積を求める( ３×３以外の部分には値も代入しない )
void MV1LoadModelToVMD_CreateMultiplyMatrixRotOnly(FMatrix *Out, FMatrix *In1, FMatrix *In2)
{
	Out->M[0][0] = In1->M[0][0] * In2->M[0][0] + In1->M[0][1] * In2->M[1][0] + In1->M[0][2] * In2->M[2][0];
	Out->M[0][1] = In1->M[0][0] * In2->M[0][1] + In1->M[0][1] * In2->M[1][1] + In1->M[0][2] * In2->M[2][1];
	Out->M[0][2] = In1->M[0][0] * In2->M[0][2] + In1->M[0][1] * In2->M[1][2] + In1->M[0][2] * In2->M[2][2];

	Out->M[1][0] = In1->M[1][0] * In2->M[0][0] + In1->M[1][1] * In2->M[1][0] + In1->M[1][2] * In2->M[2][0];
	Out->M[1][1] = In1->M[1][0] * In2->M[0][1] + In1->M[1][1] * In2->M[1][1] + In1->M[1][2] * In2->M[2][1];
	Out->M[1][2] = In1->M[1][0] * In2->M[0][2] + In1->M[1][1] * In2->M[1][2] + In1->M[1][2] * In2->M[2][2];

	Out->M[2][0] = In1->M[2][0] * In2->M[0][0] + In1->M[2][1] * In2->M[1][0] + In1->M[2][2] * In2->M[2][0];
	Out->M[2][1] = In1->M[2][0] * In2->M[0][1] + In1->M[2][1] * In2->M[1][1] + In1->M[2][2] * In2->M[2][1];
	Out->M[2][2] = In1->M[2][0] * In2->M[0][2] + In1->M[2][1] * In2->M[1][2] + In1->M[2][2] * In2->M[2][2];
}
/////////////////////////////////////
// 角度制限を判定する共通関数 (subIndexJdgの判定は割りと不明…)
void CheckLimitAngle(
	const FVector& RotMin,
	const FVector& RotMax,
	FVector * outAngle, //target angle ( in and out param)
	bool subIndexJdg //(ik link index < ik loop temp):: linkBoneIndex < ikt
	)
{
//#define DEBUG_CheckLimitAngle
#ifdef DEBUG_CheckLimitAngle
	FVector debugVec = *outAngle;
#endif
#if 0
	if (outAngle->X < RotMin.X)
	{
		float tf = 2 * RotMin.X - outAngle->X;
		outAngle->X = tf <= RotMax.X && subIndexJdg ? tf : RotMin.X;
	}
	if (outAngle->X > RotMax.X)
	{
		float tf = 2 * RotMax.X - outAngle->X;
		outAngle->X = tf >= RotMin.X && subIndexJdg ? tf : RotMax.X;
	}
	if (outAngle->Y < RotMin.Y)
	{
		float tf = 2 * RotMin.Y - outAngle->Y;
		outAngle->Y = tf <= RotMax.Y && subIndexJdg ? tf : RotMin.Y;
	}
	if (outAngle->Y > RotMax.Y)
	{
		float tf = 2 * RotMax.Y - outAngle->Y;
		outAngle->Y = tf >= RotMin.Y && subIndexJdg ? tf : RotMax.Y;
	}
	if (outAngle->Z < RotMin.Z)
	{
		float tf = 2 * RotMin.Z - outAngle->Z;
		outAngle->Z = tf <= RotMax.Z && subIndexJdg ? tf : RotMin.Z;
	}
	if (outAngle->Z > RotMax.Z)
	{
		float tf = 2 * RotMax.Z - outAngle->Z;
		outAngle->Z = tf >= RotMin.Z && subIndexJdg ? tf : RotMax.Z;
	}
#else
	*outAngle = ClampVector(
		*outAngle,
		RotMin,
		RotMax
		);
#endif
	//debug
#ifdef DEBUG_CheckLimitAngle
	UE_LOG(LogMMD4UE4_VMDFactory, Log,
		TEXT("CheckLimitAngle::out[%s]<-In[%s]:MI[%s]MX[%s]"),
		*outAngle->ToString(),
		*debugVec.ToString(),
		*RotMin.ToString(),
		*RotMax.ToString()
		);
#endif
}
//////////////////////////////////////////////////////////////////////////////////////

/*****************
* MMD側の名称からTableRowのUE側名称を検索し取得する
* Return :T is Found
* @param :ue4Name is Found Row Name
****************/
bool UVmdFactory::FindTableRowMMD2UEName(
	UDataTable* ReNameTable,
	FName mmdName,
	FName * ue4Name
	)
{
	if (ReNameTable == NULL || ue4Name == NULL)
	{
		return false;
	}

	TArray<FName> getTableNames = ReNameTable->GetRowNames();

	FMMD2UE4NameTableRow* dataRow;
	FString ContextString;
	for (int i = 0; i < getTableNames.Num(); ++i)
	{
		ContextString = "";
		dataRow = ReNameTable->FindRow<FMMD2UE4NameTableRow>(getTableNames[i], ContextString);
		if (dataRow)
		{
			if (mmdName == FName(*dataRow->MmdOriginalName))
			{
				*ue4Name = getTableNames[i];
				return true;
			}
		}
	}
	return false;
}

/*****************
* Bone名称からRefSkeltonで一致するBoneIndexを検索し取得する
* Return :index, -1 is not found
* @param :TargetName is Target Bone Name
****************/
int32 UVmdFactory::FindRefBoneInfoIndexFromBoneName(
	const FReferenceSkeleton & RefSkelton,
	const FName & TargetName
	)
{
	for (int i = 0; i < RefSkelton.GetRefBoneInfo().Num(); ++i)
	{
		if (RefSkelton.GetRefBoneInfo()[i].Name == TargetName)
		{
			return i;
		}
	}
	return -1;
}


/*****************
* 現在のキーにおける指定BoneのGlb座標を再帰的に算出する
* Return :trncform
* @param :TargetName is Target Bone Name
****************/
FTransform UVmdFactory::CalcGlbTransformFromBoneIndex(
	UAnimSequence* DestSeq,
	USkeleton* Skeleton,
	int32 BoneIndex,
	int32 keyIndex
	)
{
	if (DestSeq == NULL || Skeleton == NULL || BoneIndex < 0 || keyIndex < 0)
	{
		//error root
		return FTransform::Identity;
	}
	FTransform resultTrans(
		DestSeq->RawAnimationData[BoneIndex].RotKeys[keyIndex],
		DestSeq->RawAnimationData[BoneIndex].PosKeys[keyIndex],
		DestSeq->RawAnimationData[BoneIndex].ScaleKeys[keyIndex]
		);
	int ParentBoneIndex = Skeleton->GetReferenceSkeleton().GetParentIndex(BoneIndex);
	if (ParentBoneIndex >= 0)
	{
		//found parent bone
		resultTrans *= CalcGlbTransformFromBoneIndex(
			DestSeq,
			Skeleton,
			ParentBoneIndex,
			keyIndex
			);
	}
	return resultTrans;
}

