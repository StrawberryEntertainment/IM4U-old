// Copyright 2015 BlackMa9. All Rights Reserved.

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


	ImportUI = ConstructObject<UPmxImportUI>(UPmxImportUI::StaticClass(), this, NAME_None, RF_NoFlags);
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

	vmdMotionInfo.VMDLoaderBinary(Buffer, BufferEnd);

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
			= FText(LOCTEXT("ImportReadMe_Generic_Dbg",
			"次のImportOption用Slateはまだ実装途中です。\n\
			現時点で有効なパラメータは、\n\
			::Skeleton Asset(必須::Animation関連付け先)\n\
			::Animation Asset(NULL以外で既存AssetにMorphのみ追加する処理を実行。NULLでBoneとMorph含む新規Asset作成)\n\
			::DataTable(MMD2UE4Name) Asset(任意：NULL以外で読み込み時にBoneとMorphNameをMMD=UE4で読み替えてImportを実行する。事前にCSV形式でImportか新規作成しておく必要あり。)\n\
			です。\n\
			\n\
			注意：新規Asset生成はIKなど未対応の為非推奨。追加Morphのみ対応。"
			)
			);
		FMessageDialog::Open(EAppMsgType::Ok, MessageDbg, &TitleStr);
	}

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
	{
		FPmxImporter* PmxImporter = FPmxImporter::GetInstance();

		EPMXImportType ForcedImportType = PMXIT_StaticMesh;
		bool bOperationCanceled = false;
		bool bIsPmxFormat = true;
		// show Import Option Slate
		bool bImportAll = false;
		ImportOptions
			= GetImportOptions(
				PmxImporter,
				ImportUI,
				true,//bShowImportDialog, 
				InParent->GetPathName(),
				bOperationCanceled,
				bImportAll,
				bIsPmxFormat,
				bIsPmxFormat,
				ForcedImportType
				);
		if (ImportOptions)
		{
			Skeleton = ImportUI->Skeleton;
			////////////////////////////////////
			if (!ImportOptions->AnimSequenceAsset)
			{
				LastCreatedAnim = ImportAnimations(
					Skeleton,
					InParent,
					Name.ToString(),
					ImportUI->MMD2UE4NameTableRow,
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
	return LastCreatedAnim;
};

UAnimSequence * UVmdFactory::ImportAnimations(
	USkeleton* Skeleton,
	UObject* Outer,
	const FString& Name,
	//UFbxAnimSequenceImportData* TemplateImportData, 
	//TArray<FbxNode*>& NodeArray
	UDataTable* ReNameTable,
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
			DestSeq = ConstructObject<UAnimSequence>(UAnimSequence::StaticClass(), ParentPackage, *SequenceName, RF_Public | RF_Standalone);

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
		if (!ImportVMDToAnimSequence(LastCreatedAnim, Skeleton, ReNameTable,vmdMotionInfo))
		{
			//TBD::ERR case
			check(false);
		}
		/* morph animation regist*/
		if (!ImportMorphCurveToAnimSequence(LastCreatedAnim, Skeleton, ReNameTable,vmdMotionInfo))
		{
			//TBD::ERR case
			check(false);
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
		//TDB::ERR.  previewMesh is Null
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
		FSmartNameMapping* NameMapping 
			= Skeleton->SmartNames.GetContainer(USkeleton::AnimCurveMappingName);
		/**********************************/
		//self
		UMorphTarget * morphTargetPtr = mesh->FindMorphTarget(Name);
		if (!morphTargetPtr)
		{
			//TDB::ERR. not found Morph Target(Name) in mesh
			{
				UE_LOG(LogMMD4UE4_VMDFactory, Warning,
					TEXT("ImportMorphCurveToAnimSequence Target Morph Not Found...[%s]"),
					*Name.ToString());
			}
			continue;
		}
		/*********************************/
		// Add or retrieve curve
		USkeleton::AnimCurveUID Uid;
		NameMapping->AddOrFindName(Name, Uid);

		// FloatCurve for Morph Target 
		int CurveFlags = ACF_DrivesMorphTarget;

		FFloatCurve * CurveToImport
			= static_cast<FFloatCurve *>(DestSeq->RawCurveData.GetCurveData(Uid, FRawCurveTracks::FloatType));
		if (CurveToImport == NULL)
		{
			if (DestSeq->RawCurveData.AddCurveData(Uid, CurveFlags))
			{
				CurveToImport
					= static_cast<FFloatCurve *> (DestSeq->RawCurveData.GetCurveData(Uid, FRawCurveTracks::FloatType));
			}
			else
			{
				// this should not happen, we already checked before adding
				ensureMsg(0, TEXT("VMD Import: Critical error: no memory?"));
			}
		}
		else
		{
			CurveToImport->FloatCurve.Reset();
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
	MMD4UE4::VmdMotionInfo* vmdMotionInfo
	)
{
	if (!DestSeq || !Skeleton || !vmdMotionInfo)
	{
		//TBD:: ERR in Param...
		return false;
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
				targetName = FName(*dataRow->MmdOrignalName);
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

			//事前に各Trackに対し親Bone抜きにLocal座標で全登録予定のフレーム計算しておく（もっと良い処理があれば…検討）
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
						if (NextKey.Frame - i < 0)
						{
							blendRate = 1.0f;
						}
						else
						{ 
							//TBD::フレーム間が1だと0.5で計算されない?
							blendRate = 1.0f - (float)(NextKey.Frame - i) / (float)(NextKey.Frame - PreKey.Frame);
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
							NextKey.Bezier[0][0][0] / 127.0f, NextKey.Bezier[0][1][0] / 127.0f,
							NextKey.Bezier[1][0][0] / 127.0f, NextKey.Bezier[1][1][0] / 127.0f,
							blendRate
							)*(NextTranc.GetTranslation().X - PreTranc.GetTranslation().X) + PreTranc.GetTranslation().X
							,
							interpolateBezier(
							NextKey.Bezier[0][0][2] / 127.0f, NextKey.Bezier[0][1][2] / 127.0f,
							NextKey.Bezier[1][0][2] / 127.0f, NextKey.Bezier[1][1][2] / 127.0f,
							blendRate
							)*(NextTranc.GetTranslation().Y - PreTranc.GetTranslation().Y) + PreTranc.GetTranslation().Y
							,
							interpolateBezier(
							NextKey.Bezier[0][0][1] / 127.0f, NextKey.Bezier[0][1][1] / 127.0f,
							NextKey.Bezier[1][0][1] / 127.0f, NextKey.Bezier[1][1][1] / 127.0f,
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
						NowTranc.SetRotation(
							FQuat(
							interpolateBezier(
							NextKey.Bezier[0][0][0] / 127.0f, NextKey.Bezier[0][1][0] / 127.0f,
							NextKey.Bezier[1][0][0] / 127.0f, NextKey.Bezier[1][1][0] / 127.0f,
							blendRate
							)*(NextTranc.GetRotation().X - PreTranc.GetRotation().X) + PreTranc.GetRotation().X
							,
							interpolateBezier(
							NextKey.Bezier[0][0][2] / 127.0f, NextKey.Bezier[0][1][2] / 127.0f,
							NextKey.Bezier[1][0][2] / 127.0f, NextKey.Bezier[1][1][2] / 127.0f,
							blendRate
							)*(NextTranc.GetRotation().Y - PreTranc.GetRotation().Y) + PreTranc.GetRotation().Y
							,
							interpolateBezier(
							NextKey.Bezier[0][0][1] / 127.0f, NextKey.Bezier[0][1][1] / 127.0f,
							NextKey.Bezier[1][0][1] / 127.0f, NextKey.Bezier[1][1][1] / 127.0f,
							blendRate
							)*(NextTranc.GetRotation().Z - PreTranc.GetRotation().Z) + PreTranc.GetRotation().Z
							,
							interpolateBezier(
							NextKey.Bezier[0][0][3] / 127.0f, NextKey.Bezier[0][1][3] / 127.0f,
							NextKey.Bezier[1][0][3] / 127.0f, NextKey.Bezier[1][1][3] / 127.0f,
							blendRate
							)*(NextTranc.GetRotation().W - PreTranc.GetRotation().W) + PreTranc.GetRotation().W
							)
							);
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
							//check(false);
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
	{
		//TBD::ここにIK再計算処理を入れる
	}
	GWarn->EndSlowTask();
	return true;
}


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
			if (mmdName == FName(*dataRow->MmdOrignalName))
			{
				*ue4Name = getTableNames[i];
				return true;
			}
		}
	}
	return false;
}