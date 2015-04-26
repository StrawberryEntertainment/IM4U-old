
// Copyright 2015 BlackMa9. All Rights Reserved.
#pragma once


#include "Engine.h"
#include "MMDImportHelper.h"


// Copy From DxLib DxModelLoaderVMD.h
// DX Library Copyright (C) 2001-2008 Takumi Yamada.

//MMD�̃��[�V�����f�[�^(VMD)�`���@�߂�2�@(�\���EIK)
//http://blog.goo.ne.jp/torisu_tetosuki/e/2a2cb5c2de7563c5e6f20b19e1fe6139

//#define BYTE (unsigned char)

namespace MMD4UE4
{

	struct VMD_HEADER
	{
		char header[30];	// Vocaloid Motion Data 0002
		char modelName[20];	// Target Model Name
	};

	// VMD�L�[�f�[�^( 111byte )
	struct VMD_KEY
	{
		//BYTE	Data[111];
		
		char	Name[ 15 ] ;						// ���O
		uint32	Frame ;								// �t���[��
		float	Position[ 3 ] ;						// ���W
		float	Quaternion[ 4 ] ;					// �N�H�[�^�j�I��
		/*float	PosXBezier[ 4 ] ;					// ���W�w�p�x�W�F�Ȑ����
		float	PosYBezier[ 4 ] ;					// ���W�x�p�x�W�F�Ȑ����
		float	PosZBezier[ 4 ] ;					// ���W�y�p�x�W�F�Ȑ����
		float	RotBezier[4 ] ;					// ��]�p�x�W�F�Ȑ����
		*/
		BYTE	Bezier[2][2][4];					// [id] [xy][XYZ R]

	};

	// VMD�\��L�[�f�[�^( 23byte )
	struct VMD_FACE_KEY
	{
		/*BYTE	Data[23];						// �f�[�^
		*/
		char	Name[ 15 ] ;						// �\�
		uint32	Frame ;								// �t���[��
		float	Factor ;							// �u�����h��
		
	};

	// VMD�J�����L�[�f�[�^
	struct VMD_CAMERA
	{
		/*BYTE	Data[61];						// �f�[�^
		*/
		uint32	FrameNo;							//  4:  0:�t���[���ԍ�
		float	Length;								//  8:  4: -(����)
		float	Location[3];						// 20:  8:�ʒu
		float	Rotate[3];							// 32: 20:�I�C���[�p // X���͕��������]���Ă���̂Œ���
		BYTE	Interpolation[24];					// 56: 32:��ԏ�� // �����炭[6][4](������)
		uint32	ViewingAngle;						// 60: 56:����
		BYTE	Perspective;						// 61: 60:�ˉe�J�������ǂ��� 0:�ˉe�J���� 1:���ˉe�J����
		
	};
	// VMD �Ɩ�
	struct VMD_LIGHT
	{
		uint32	flameNo;
		float	RGB[3];
		float	Loc[3];
	};
	// VMD�Z���t�V���h�[
	struct VMD_SELFSHADOW{
		uint32	FlameNo;
		BYTE	Mode;
		float	Distance;
	};
	////////////////////////////////////////////////////
	// mmd 7.40�ȍ~�ō쐬���ꂽVMD�̏ꍇ�ȉ��̊g���f�[�^������
	// �Q�l�FMMD�̃��[�V�����f�[�^(VMD)�`���@�߂�2�@(�\���EIK)
	///////////////////////////////////////////////////
	// �\���EIK�f�[�^
	//int dataCount; // �f�[�^��
	//VMD_VISIBLE_IK_DATA data[dataCount];
	////////////
	// IK�f�[�^�p�\����
	typedef struct _VMD_IK_DATA
	{
		char ikBoneName[20]; // IK�{�[����
		unsigned char ikEnabled; // IK�L�� // 0:off 1:on
	} VMD_IK_DATA;
	// �\���EIK�f�[�^�p�\����
	typedef struct _VMD_VISIBLE_IK_DATA
	{
		int frameNo; // �t���[���ԍ�
		unsigned char visible; // �\�� // 0:off 1:on
		int ikCount; // IK��
		TArray<VMD_IK_DATA> ikData; // ikData[ikCount]; // IK�f�[�^���X�g
	} VMD_VISIBLE_IK_DATA;
	////////////////////////////////////////////////////

	//VMD Motion Data for Read-
	struct VmdReadMotionData
	{
		VMD_HEADER				vmdHeader;
		//Bone
		int32					vmdKeyCount;
		TArray<VMD_KEY>			vmdKeyList;
		//Skin
		int32					vmdFaceCount;
		TArray<VMD_FACE_KEY>	vmdFaceList;
		//camera 
		int32					vmdCameraCount;
		TArray<VMD_CAMERA>		vmdCameraList;
		//Light
		int32					vmdLightCount;
		TArray<VMD_LIGHT>		vmdLightList;
		//Self Shadow
		int32					vmdSelfShadowCount;
		TArray<VMD_SELFSHADOW>	vmdSelfShadowList;
		//extend info, ik visible
		int32					vmdExtIKCount;
		TArray<VMD_VISIBLE_IK_DATA>	vmdExtIKList;
	};

	struct VmdKeyTrackList
	{
		// Track Name
		FString					TrackName;
		// min Frame
		uint32					minFrameCount;
		// max Frame
		uint32					maxFrameCount;
		// Sorted Key Frame Data List
		TArray<VMD_KEY>			keyList;
		// sort frame num
		TArray<int32>			sortIndexList;
	};
	struct VmdFaceTrackList
	{
		// Track Name
		FString					TrackName;
		// min Frame
		uint32					minFrameCount;
		// max Frame
		uint32					maxFrameCount;
		// Sorted Key Frame Data List
		TArray<VMD_FACE_KEY>	keyList;
		// sort frame num
		TArray<int32>			sortIndexList;
	};
	//
	DECLARE_LOG_CATEGORY_EXTERN(LogMMD4UE4_VmdMotionInfo, Log, All)
	// Inport�p meta data �i�[�N���X
	class VmdMotionInfo : public MMDImportHelper
	{
	public:
		VmdMotionInfo();
		~VmdMotionInfo();

		///////////////////////////////////////
		bool VMDLoaderBinary(
			const uint8 *& Buffer,
			const uint8 * BufferEnd
			);

		///////////////////////////////////////
		bool ConvertVMDFromReadData(
			VmdReadMotionData * readData
			);


		//////////////////////////////////////////
		enum EVMDKEYFRAMETYPE
		{
			EVMD_KEYBONE,
			EVMD_KEYFACE,
			EVMD_KEYCAM //�g�����s���A�\��
		};
		//////////////////////////////////////////
		// �w��List���ŊY������Frame��������΂���Index�l��Ԃ��B�ُ�l=-1�B
		int32 FindKeyTrackName(
			FString targetName,
			EVMDKEYFRAMETYPE listType);

		//////////////////////////////////////////

		// vmd Target Model Name
		FString					ModelName;
		// min Frame 
		uint32					minFrame;
		// max Frams
		uint32					maxFrame;

		//Keys
		TArray<VmdKeyTrackList>	keyBoneList;
		//Skins
		TArray<VmdFaceTrackList> keyFaceList;
	};

}