
#include "IM4UPrivatePCH.h"
#include "PmdImporter.h"
#include "EncodeHelper.h"

namespace MMD4UE4
{

	DEFINE_LOG_CATEGORY(LogMMD4UE4_PmdMeshInfo)

		PmdMeshInfo::PmdMeshInfo()
	{
	}


	PmdMeshInfo::~PmdMeshInfo()
	{
	}


	FVector PmdMeshInfo::ConvertVectorAsixToUE4FromMMD(FVector vec)
	{
#if 1 //For UE4 From MMD
		FVector temp;
		temp.Y = vec.Z*(-1);
		temp.X = vec.X*(1);
		temp.Z = vec.Y*(1);
		return temp;
#else
		return vec;
#endif
	}



	bool PmdMeshInfo::PMDLoaderBinary(
		const uint8 *& Buffer,
		const uint8 * BufferEnd
		)
	{
		////////////////////////////////////////////

		uint32 memcopySize = 0;
		float modelScale = 10.0f;

		memcopySize = sizeof(header);
		FMemory::Memcpy(&header, Buffer, memcopySize);
		// PMD�t�@�C�����ǂ������m�F
		if (header.Magic[0] == 'P' && header.Magic[1] == 'm' && header.Magic[2] == 'd')
		{
			UE_LOG(LogMMD4UE4_PmdMeshInfo, Warning, TEXT("PMX Import START /Correct Magic[PMX]"));
		}
		else
		{
			UE_LOG(LogMMD4UE4_PmdMeshInfo, Error, TEXT("PMX Import FALSE/Return /UnCorrect Magic[PMX]"));
			return false;
		}
		// �o�[�W�����P�ȊO�͓ǂݍ��߂Ȃ�
		/*if (*((float *)header.Version) != 0x0000803f)
		{
			//DXST_ERRORLOGFMT_ADD((_T("PMD Load Error : �o�[�W�����P�D�O�ȊO�͓ǂݍ��߂܂���\n")));
			return false;
		}*/
		Buffer += memcopySize;
		// �e�f�[�^�̐擪�A�h���X���Z�b�g
		{

			memcopySize = sizeof(PMD_VERTEX_DATA);
			FMemory::Memcpy(&vertexData, Buffer, memcopySize);
			Buffer += memcopySize;

			memcopySize = 38;// sizeof(PMD_VERTEX);
			vertexList.AddZeroed(vertexData.Count); 
			for (int32 i = 0; i < vertexData.Count; ++i)
			{
				FMemory::Memcpy(&vertexList[i], Buffer, memcopySize);
				Buffer += memcopySize;
			}

		}
		{

			memcopySize = sizeof(PMD_FACE_DATA);
			FMemory::Memcpy(&faceData, Buffer, memcopySize);
			Buffer += memcopySize;

			memcopySize = sizeof(PMD_FACE);
			faceList.AddZeroed(faceData.VertexCount / 3);
			for (uint32 i = 0; i < faceData.VertexCount / 3; ++i)
			{
				FMemory::Memcpy(&faceList[i], Buffer, memcopySize);
				Buffer += memcopySize;
			}

		}
		{

			memcopySize = sizeof(PMD_MATERIAL_DATA);
			FMemory::Memcpy(&materialData, Buffer, memcopySize);
			Buffer += memcopySize;

			//memcopySize = 70;//sizeof(PMD_MATERIAL);
			materialList.AddZeroed(materialData.Count);
			for (int32 i = 0; i < materialData.Count; ++i)
			{
				memcopySize = sizeof(float) * 11 + sizeof(BYTE) * 2;//DiffuseR --> Edge;
				FMemory::Memcpy(&materialList[i], Buffer, memcopySize);
				Buffer += memcopySize;
				memcopySize = sizeof(int) * 1 + sizeof(char) * 20;//FaceVertexCount --> TextureFileName[20];
				FMemory::Memcpy(&(materialList[i].FaceVertexCount), Buffer, memcopySize);
				Buffer += memcopySize;
			}

		}
		{

			memcopySize = sizeof(PMD_BONE_DATA);
			FMemory::Memcpy(&boneData, Buffer, memcopySize);
			Buffer += memcopySize;

			//memcopySize = 39;//sizeof(PMD_BONE);
			boneList.AddZeroed(boneData.Count);
			for (int32 i = 0; i < boneData.Count; ++i)
			{
				memcopySize =
					sizeof(char) * 20
					+ sizeof(uint16) * 2
					+ sizeof(BYTE) * 1
					+ sizeof(uint16) * 1;//name --> IkParent;
				FMemory::Memcpy(&boneList[i], Buffer, memcopySize);
				Buffer += memcopySize;
				memcopySize =
					sizeof(float) * 3;//name --> IkParent;
				FMemory::Memcpy(&(boneList[i].HeadPos[0]), Buffer, memcopySize);
				Buffer += memcopySize;
			}

		}
		{

			memcopySize = sizeof(PMD_IK_DATA);
			FMemory::Memcpy(&ikData, Buffer, memcopySize);
			Buffer += memcopySize;

			//memcopySize = 39;//sizeof(PMD_BONE);
			ikList.AddZeroed(ikData.Count);
			for (int32 i = 0; i < ikData.Count; ++i)
			{
				memcopySize = 0
					+ sizeof(uint16) * 2
					+ sizeof(BYTE) * 1
					+ sizeof(uint16) * 1
					+ sizeof(float) * 1;//Bone --> ControlWeight;
				FMemory::Memcpy(&ikList[i], Buffer, memcopySize);
				Buffer += memcopySize;

				BYTE tempChainLength = ikList[i].Data[2 + 2];//Byte ChainLength;
				ikList[i].ChainBoneIndexs.AddZeroed(tempChainLength);
				memcopySize = sizeof(uint16);// *tempChainLength;
				for (int32 k = 0; k < tempChainLength; ++k)
				{
					FMemory::Memcpy(&(ikList[i].ChainBoneIndexs[k]), Buffer, memcopySize);
					Buffer += memcopySize;
				}
			}

		}
		{

			memcopySize = sizeof(PMD_SKIN_DATA);
			FMemory::Memcpy(&skinData, Buffer, memcopySize);
			Buffer += memcopySize;

			//memcopySize = 39;//sizeof(PMD_BONE);
			skinList.AddZeroed(skinData.Count);
			for (int32 i = 0; i < skinData.Count; ++i)
			{
				memcopySize = 0
					+ sizeof(char) * 20
					+ sizeof(int) * 1
					+ sizeof(BYTE) * 1;//Name --> SkinType;
				FMemory::Memcpy(&skinList[i], Buffer, memcopySize);
				Buffer += memcopySize;

				int tempVertexCount = skinList[i].VertexCount;
				skinList[i].Vertex.AddZeroed(tempVertexCount);
				memcopySize = sizeof(PMD_SKIN_VERT);// *tempVertexCount;
				for (int32 k = 0; k < tempVertexCount; ++k)
				{
					FMemory::Memcpy(&(skinList[i].Vertex[k]), Buffer, memcopySize);
					Buffer += memcopySize;
				}
			}

		}
		/*{

			PmdIKNum = *((WORD *)Src);
			Src += 2;

			PmdIK = (PMD_IK *)Src;
			}*/
		//////////////////////////////////////////////
		//UE_LOG(LogMMD4UE4_PmdMeshInfo, Warning, TEXT("PMX Importer Class Complete"));
		return true;
	}

	//////////////////////////////////////
	// from PMD/PMX Binary Buffer To String @ TextBuf
	// 4 + n: TexBuf
	// buf : top string (top data)
	// encodeType : 0 utf-16, 1 utf-8
	//////////////////////////////////////
	FString PmdMeshInfo::PMDTexBufferToFString(const uint8 ** buffer, const uint32 size)
	{
		FString NewString;
		//uint32 size = 0;
		//temp data 
		TArray<char> RawModData;

		{
			//FMemory::Memcpy(&size, *buffer, sizeof(uint32));
			//*buffer += sizeof(uint32);
			//size = 20;
			RawModData.Empty(size);
			RawModData.AddUninitialized(size);
			FMemory::Memcpy(RawModData.GetData(), *buffer, RawModData.Num());
			RawModData.Add(0); RawModData.Add(0);
			NewString.Append(UTF8_TO_TCHAR(encodeHelper.convert_encoding(RawModData.GetData(), "shift-jis", "utf-8").c_str()));
			//NewString.Append(RawModData.GetData());
			*buffer += size;
		}
		return NewString;
	}
	FString PmdMeshInfo::ConvertPMDSJISToFString(
		uint8 *buffer,
		const uint32 size
		)
	{
		FString NewString;
		//uint32 size = 0;
		//temp data 
		TArray<char> RawModData;

		{
			//FMemory::Memcpy(&size, *buffer, sizeof(uint32));
			//*buffer += sizeof(uint32);
			//size = 20;
			RawModData.Empty(size);
			RawModData.AddUninitialized(size);
			FMemory::Memcpy(RawModData.GetData(), buffer, RawModData.Num());
			RawModData.Add(0); RawModData.Add(0);
			NewString.Append(UTF8_TO_TCHAR(encodeHelper.convert_encoding(RawModData.GetData(), "shift-jis", "utf-8").c_str()));
			//NewString.Append(RawModData.GetData());
		}
		return NewString;
	}
	/////////////////////////////////////

	bool PmdMeshInfo::ConvertToPmxFormat(
		PmxMeshInfo * pmxMeshInfoPtr 
		)
	{
		uint32 memcopySize = 0;
		float modelScale = 10.0f;
		PmdMeshInfo * pmdMeshInfoPtr = this;

		pmxMeshInfoPtr->modelNameJP 
			= ConvertPMDSJISToFString((uint8 *)&(header.Name), sizeof(header.Name));

		pmxMeshInfoPtr->modelNameJP 
			= pmxMeshInfoPtr->modelNameJP.Replace(TEXT("."), TEXT("_"));// [.] is broken filepath for ue4 

		{
			pmxMeshInfoPtr->vertexList.AddZeroed(vertexData.Count);
			for (int32 VertexIndex = 0; VertexIndex < vertexData.Count; ++VertexIndex)
			{
				PMD_VERTEX & pmdVertexPtr = vertexList[VertexIndex];
				PMX_VERTEX & pmxVertexPtr = pmxMeshInfoPtr->vertexList[VertexIndex];
				///
				//�ʒu(x,y,z)
				memcopySize = sizeof(pmxVertexPtr.Position);
				FMemory::Memcpy(&pmxVertexPtr.Position, pmdVertexPtr.Position, memcopySize);
				pmxVertexPtr.Position = ConvertVectorAsixToUE4FromMMD(pmxVertexPtr.Position)*modelScale;
				//�@��(x,y,z)
				memcopySize = sizeof(pmxVertexPtr.Normal);
				FMemory::Memcpy(&pmxVertexPtr.Normal, pmdVertexPtr.Normal, memcopySize);
				pmxVertexPtr.Normal = ConvertVectorAsixToUE4FromMMD(pmxVertexPtr.Normal);
				//UV(u,v)
				memcopySize = sizeof(pmxVertexPtr.UV);
				FMemory::Memcpy(&pmxVertexPtr.UV, pmdVertexPtr.Uv, memcopySize);
				/*
				float tempUV = pmxVertexPtr.UV.X;//UE4���W�n���]
				pmxVertexPtr.UV.X = 1 - pmxVertexPtr.UV.Y;
				pmxVertexPtr.UV.Y = 1 - tempUV;
				*/
				//�ǉ�UV(x,y,z,w)  PMX�w�b�_�̒ǉ�UV���ɂ��	n:�ǉ�UV�� 0�`4

				// �E�F�C�g�ό`���� 0:BDEF1 1:BDEF2 2:BDEF4 3:SDEF
				pmxVertexPtr.WeightType = 0;
				//
				if (true)
				{
					pmxVertexPtr.BoneIndex[0] = pmdVertexPtr.BoneNo[0]+1;
					pmxVertexPtr.BoneIndex[1] = pmdVertexPtr.BoneNo[1]+1;
					pmxVertexPtr.BoneWeight[0] = pmdVertexPtr.BoneWeight;
					pmxVertexPtr.BoneWeight[1] = 1 - pmdVertexPtr.BoneWeight;
				}
				else
				{
					UE_LOG(LogMMD4UE4_PmxMeshInfo, Error, TEXT("PMX Import FALSE/Return /UnCorrect EncodeType"));
				}
				//�G�b�W�{��  �ގ��̃G�b�W�T�C�Y�ɑ΂��Ă̔{���l
			}
		}
		{
			/*
			����

			n : ���_Index�T�C�Y     | ���_�̎Q��Index

			��3�_(3���_Index)��1��
			�ގ����̖ʐ��͍ގ����̖�(���_)���ŊǗ� (��PMD����)
			*/
			uint32 PmxFaceNum = 0;
			PmxFaceNum = faceData.VertexCount;
			PmxFaceNum /= 3;

			pmxMeshInfoPtr->faseList.AddZeroed(PmxFaceNum);
			for (uint32 FaceIndex = 0; FaceIndex < PmxFaceNum; ++FaceIndex)
			{
				PMD_FACE & pmdFaceListPtr = faceList[FaceIndex];
				PMX_FACE & pmxFaseListPtr = pmxMeshInfoPtr->faseList[FaceIndex];
				//
				for (int SubNum = 0; SubNum < 3; ++SubNum)
				{
					pmxFaseListPtr.VertexIndex[SubNum]
						= pmdFaceListPtr.VertexIndx[SubNum];
				}
			}
		}
		/*
		{

			// �e�N�X�`���̐����擾
			uint32 PmxTextureNum = 0;
			//
			memcopySize = sizeof(PmxTextureNum);
			FMemory::Memcpy(&PmxTextureNum, Buffer, memcopySize);
			Buffer += memcopySize;

			// �e�N�X�`���f�[�^���i�[���郁�����̈�̊m��
			textureList.AddZeroed(PmxTextureNum);

			// �e�N�X�`���̏����擾
			for (uint32 i = 0; i < PmxTextureNum; i++)
			{
				textureList[i].TexturePath = PMXTexBufferToFString(&Buffer, pmxEncodeType);
			}
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning, TEXT("PMX Import [textureList] Complete"));
		}
		*/
		{
			FString tempAlphaStr;
			TArray<FString> tempTexPathList;
			// �}�e���A���̐����擾
			uint32 PmxMaterialNum = 0;
			//
			PmxMaterialNum = materialData.Count;

			// �}�e���A���f�[�^���i�[���郁�����̈�̊m��
			pmxMeshInfoPtr->materialList.AddZeroed(PmxMaterialNum);

			// �}�e���A���̓ǂ݂���
			for (uint32 i = 0; i < PmxMaterialNum; i++)
			{
				PMD_MATERIAL & pmdMaterialPtr = materialList[i];
				PMX_MATERIAL & pmxMaterialPtr = pmxMeshInfoPtr->materialList[i];

				// �ގ����̎擾
				pmxMaterialPtr.Name = FString::Printf(TEXT("mat_%d"), i);
				pmxMaterialPtr.NameEng = pmxMaterialPtr.Name;

				//Diffuse (R,G,B,A)
				pmxMaterialPtr.Diffuse[0] = pmdMaterialPtr.DiffuseR;
				pmxMaterialPtr.Diffuse[1] = pmdMaterialPtr.DiffuseG;
				pmxMaterialPtr.Diffuse[2] = pmdMaterialPtr.DiffuseB;
				pmxMaterialPtr.Diffuse[3] = pmdMaterialPtr.Alpha;
				//Specular (R,G,B))
				pmxMaterialPtr.Specular[0] = pmdMaterialPtr.SpecularR;
				pmxMaterialPtr.Specular[1] = pmdMaterialPtr.SpecularG;
				pmxMaterialPtr.Specular[2] = pmdMaterialPtr.SpecularB;
				//Specular�W��
				pmxMaterialPtr.SpecularPower = pmdMaterialPtr.Specularity;
				//Ambient (R,G,B)
				pmxMaterialPtr.Ambient[0] = pmdMaterialPtr.AmbientR;
				pmxMaterialPtr.Ambient[1] = pmdMaterialPtr.AmbientG;
				pmxMaterialPtr.Ambient[2] = pmdMaterialPtr.AmbientB;


				/*
				�`��t���O(8bit) - �ebit 0:OFF 1:ON
				0x01:���ʕ`��, 0x02:�n�ʉe, 0x04:�Z���t�V���h�E�}�b�v�ւ̕`��, 0x08:�Z���t�V���h�E�̕`��,
				0x10:�G�b�W�`��
				*/
				tempAlphaStr = FString::Printf(TEXT("%.03f"), pmdMaterialPtr.Alpha);
				pmxMaterialPtr.CullingOff = (pmdMaterialPtr.Alpha < 1.0f ) ? 1 : 0;
				pmxMaterialPtr.GroundShadow = (0) ? 1 : 0;
				pmxMaterialPtr.SelfShadowMap = tempAlphaStr.Equals("0.980") ? 1 : 0;
				pmxMaterialPtr.SelfShadowDraw = tempAlphaStr.Equals( "0.980") ? 1 : 0;
				pmxMaterialPtr.EdgeDraw = (0) ? 1 : 0;

				//�G�b�W�F (R,G,B,A)
				pmxMaterialPtr.EdgeColor[0] = 0;
				pmxMaterialPtr.EdgeColor[1] = 0;
				pmxMaterialPtr.EdgeColor[2] = 0;
				pmxMaterialPtr.EdgeColor[3] = 0;

				//�G�b�W�T�C�Y
				pmxMaterialPtr.EdgeSize = 0;

				
				PMX_TEXTURE tempTex;
				FString tempTexPathStr;
				FString tempShaPathStr;
				tempTex.TexturePath
					= ConvertPMDSJISToFString(
						(uint8 *)pmdMaterialPtr.TextureFileName,
						sizeof(pmdMaterialPtr.TextureFileName));
				if (tempTex.TexturePath.Split("/", &tempTexPathStr, &tempShaPathStr))
				{
					tempTex.TexturePath = tempTexPathStr;
					pmxMaterialPtr.SphereMode = 2;
				}
				else if (tempTex.TexturePath.Split("*", &tempTexPathStr, &tempShaPathStr))
				{

					tempTex.TexturePath = tempTexPathStr;
					pmxMaterialPtr.SphereMode = 1;
				}
				else
				{
					//tempTex.TexturePath = tempTexPathStr;
					tempShaPathStr = "";
				}
				//�ʏ�e�N�X�`��, �e�N�X�`���e�[�u���̎Q��Index
				pmxMaterialPtr.TextureIndex
					= tempTexPathList.AddUnique(pmdMaterialPtr.TextureFileName);
					//pmxMeshInfoPtr->textureList.Add/*Unique*/(tempTex);
				if (pmxMaterialPtr.TextureIndex > pmxMeshInfoPtr->textureList.Num() - 1)
				{
					pmxMeshInfoPtr->textureList.Add(tempTex);
				}
				//�X�t�B�A�e�N�X�`��, �e�N�X�`���e�[�u���̎Q��Index  ���e�N�X�`���g���q�̐����Ȃ�
				pmxMaterialPtr.SphereTextureIndex
					= 0;
				//�X�t�B�A���[�h 0:���� 1:��Z(sph) 2:���Z(spa) 
				//3:�T�u�e�N�X�`��(�ǉ�UV1��x,y��UV�Q�Ƃ��Ēʏ�e�N�X�`���`����s��)
				pmxMaterialPtr.SphereMode = 0;
				//���LToon�t���O 0:�p���l�͌�Toon 1 : �p���l�͋��LToon
				pmxMaterialPtr.ToonFlag = 1;

				if (pmxMaterialPtr.ToonFlag == 0)
				{//Toon�e�N�X�`��, �e�N�X�`���e�[�u���̎Q��Index
					pmxMaterialPtr.ToonTextureIndex
						= 0;
				}
				else
				{//���LToon�e�N�X�`��[0�`9] -> ���ꂼ�� toon01.bmp�`toon10.bmp �ɑΉ�
					pmxMaterialPtr.ToonTextureIndex = pmdMaterialPtr.ToolImage;
				}

				// �����̓X�L�b�v

				//�ގ��ɑΉ������(���_)�� (�K��3�̔{���ɂȂ�)
				pmxMaterialPtr.MaterialFaceNum = pmdMaterialPtr.FaceVertexCount;
			}
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning, TEXT("PMX Import [materialList] Complete"));
		}
		{
			// ���ׂĂ̐eBone��UE4�����ɒǉ�
			// �{�[���f�[�^���i�[���郁�����̈�̊m��
			pmxMeshInfoPtr->boneList.AddZeroed(1);

			// �{�[���̐����擾
			uint32 PmxBoneNum = 0;
			uint32 offsetBoneIndx = 1;

			// �{�[�����̎擾
			{
				pmxMeshInfoPtr->boneList[PmxBoneNum].Name = "AllTopRootBone";
				pmxMeshInfoPtr->boneList[PmxBoneNum].NameEng = "AllTopRootBone";
				pmxMeshInfoPtr->boneList[PmxBoneNum].Position = FVector(0);
				pmxMeshInfoPtr->boneList[PmxBoneNum].ParentBoneIndex = INDEX_NONE;
			}
			PmxBoneNum = boneData.Count;
			// �{�[���f�[�^���i�[���郁�����̈�̊m��
			pmxMeshInfoPtr->boneList.AddZeroed(PmxBoneNum);

			// �{�[�����̎擾
			uint32 PmxIKNum = 0;
			for (uint32 i = 1; i < PmxBoneNum + offsetBoneIndx; i++)
			{
				PMD_BONE & pmdBonePtr = boneList[i-1];
				PMX_BONE & pmxBonePtr = pmxMeshInfoPtr->boneList[i];

				pmxBonePtr.Name
					= ConvertPMDSJISToFString((uint8 *)&(pmdBonePtr.Name),sizeof(pmdBonePtr.Name));
				pmxBonePtr.NameEng
					= pmxBonePtr.Name;
				//
				memcopySize = sizeof(pmxBonePtr.Position);
				FMemory::Memcpy(&pmxBonePtr.Position, pmdBonePtr.HeadPos, memcopySize);
				pmxBonePtr.Position = ConvertVectorAsixToUE4FromMMD(pmxBonePtr.Position) * modelScale;

				pmxBonePtr.ParentBoneIndex
					= pmdBonePtr.Parent + offsetBoneIndx;
#if 0
				//
				memcopySize = sizeof(pmxBonePtr.TransformLayer);
				FMemory::Memcpy(&pmxBonePtr.TransformLayer, Buffer, memcopySize);
				Buffer += memcopySize;

				WORD Flag;
				//
				memcopySize = sizeof(Flag);
				FMemory::Memcpy(&Flag, Buffer, memcopySize);
				Buffer += memcopySize;

				pmxBonePtr.Flag_LinkDest = (Flag & 0x0001) != 0 ? 1 : 0;
				pmxBonePtr.Flag_EnableRot = (Flag & 0x0002) != 0 ? 1 : 0;
				pmxBonePtr.Flag_EnableMov = (Flag & 0x0004) != 0 ? 1 : 0;
				pmxBonePtr.Flag_Disp = (Flag & 0x0008) != 0 ? 1 : 0;
				pmxBonePtr.Flag_EnableControl = (Flag & 0x0010) != 0 ? 1 : 0;
				pmxBonePtr.Flag_IK = (Flag & 0x0020) != 0 ? 1 : 0;
				pmxBonePtr.Flag_AddRot = (Flag & 0x0100) != 0 ? 1 : 0;
				pmxBonePtr.Flag_AddMov = (Flag & 0x0200) != 0 ? 1 : 0;
				pmxBonePtr.Flag_LockAxis = (Flag & 0x0400) != 0 ? 1 : 0;
				pmxBonePtr.Flag_LocalAxis = (Flag & 0x0800) != 0 ? 1 : 0;
				pmxBonePtr.Flag_AfterPhysicsTransform = (Flag & 0x1000) != 0 ? 1 : 0;
				pmxBonePtr.Flag_OutParentTransform = (Flag & 0x2000) != 0 ? 1 : 0;

				if (pmxBonePtr.Flag_LinkDest == 0)
				{
					//
memcopySize = sizeof(pmxBonePtr.OffsetPosition);
FMemory::Memcpy(&pmxBonePtr.OffsetPosition, Buffer, memcopySize);
pmxBonePtr.OffsetPosition = ConvertVectorAsixToUE4FromMMD(pmxBonePtr.OffsetPosition) *modelScale;
Buffer += memcopySize;
				}
				else
				{
					pmxBonePtr.LinkBoneIndex
						= PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
				}

				if (pmxBonePtr.Flag_AddRot == 1 || pmxBonePtr.Flag_AddMov == 1)
				{
					pmxBonePtr.AddParentBoneIndex
						= PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
					//
					memcopySize = sizeof(pmxBonePtr.AddRatio);
					FMemory::Memcpy(&pmxBonePtr.AddRatio, Buffer, memcopySize);
					Buffer += memcopySize;
				}

				if (pmxBonePtr.Flag_LockAxis == 1)
				{
					//
					memcopySize = sizeof(pmxBonePtr.LockAxisVector);
					FMemory::Memcpy(&pmxBonePtr.LockAxisVector, Buffer, memcopySize);
					Buffer += memcopySize;
				}

				if (pmxBonePtr.Flag_LocalAxis == 1)
				{

					//
					memcopySize = sizeof(pmxBonePtr.LocalAxisXVector);
					FMemory::Memcpy(&pmxBonePtr.LocalAxisXVector, Buffer, memcopySize);
					Buffer += memcopySize;

					//
					memcopySize = sizeof(pmxBonePtr.LocalAxisZVector);
					FMemory::Memcpy(&pmxBonePtr.LocalAxisZVector, Buffer, memcopySize);
					Buffer += memcopySize;
				}

				if (pmxBonePtr.Flag_OutParentTransform == 1)
				{
					//
					memcopySize = sizeof(pmxBonePtr.OutParentTransformKey);
					FMemory::Memcpy(&pmxBonePtr.OutParentTransformKey, Buffer, memcopySize);
					Buffer += memcopySize;
				}

				if (pmxBonePtr.Flag_IK == 1)
				{
					PmxIKNum++;

					pmxBonePtr.IKInfo.TargetBoneIndex
						= PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
					//
					memcopySize = sizeof(pmxBonePtr.IKInfo.LoopNum);
					FMemory::Memcpy(&pmxBonePtr.IKInfo.LoopNum, Buffer, memcopySize);
					Buffer += memcopySize;

					//
					memcopySize = sizeof(pmxBonePtr.IKInfo.RotLimit);
					FMemory::Memcpy(&pmxBonePtr.IKInfo.RotLimit, Buffer, memcopySize);
					Buffer += memcopySize;

					//
					memcopySize = sizeof(pmxBonePtr.IKInfo.LinkNum);
					FMemory::Memcpy(&pmxBonePtr.IKInfo.LinkNum, Buffer, memcopySize);
					Buffer += memcopySize;
					if (pmxBonePtr.IKInfo.LinkNum >= PMX_MAX_IKLINKNUM)
					{
						return false;
					}

					for (int32 j = 0; j < pmxBonePtr.IKInfo.LinkNum; j++)
					{
						pmxBonePtr.IKInfo.Link[j].BoneIndex
							= PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
						//
						memcopySize = sizeof(pmxBonePtr.IKInfo.Link[j].RotLockFlag);
						FMemory::Memcpy(&pmxBonePtr.IKInfo.Link[j].RotLockFlag, Buffer, memcopySize);
						Buffer += memcopySize;

						if (pmxBonePtr.IKInfo.Link[j].RotLockFlag == 1)
						{
							//
							memcopySize = sizeof(pmxBonePtr.IKInfo.Link[j].RotLockMin);
							FMemory::Memcpy(&pmxBonePtr.IKInfo.Link[j].RotLockMin[0], Buffer, memcopySize);
							Buffer += memcopySize;
							//
							memcopySize = sizeof(pmxBonePtr.IKInfo.Link[j].RotLockMax);
							FMemory::Memcpy(&pmxBonePtr.IKInfo.Link[j].RotLockMax[0], Buffer, memcopySize);
							Buffer += memcopySize;
						}
					}
				}
#endif
			}
			{
				//IK
			}
			{
				int32 i, j;
				// ���[�t���̐����擾
				int32 PmxMorphNum = 0;
				PmxMorphNum = pmdMeshInfoPtr->skinData.Count;

				// ���[�t�f�[�^���i�[���郁�����̈�̊m��
				pmxMeshInfoPtr->morphList.AddZeroed(PmxMorphNum);

				// ���[�t���̓ǂݍ���
				int32 PmxSkinNum = 0;
				for (i = 0; i < PmxMorphNum; i++)
				{
					pmxMeshInfoPtr->morphList[i].Name
						= ConvertPMDSJISToFString(
							(uint8 *)&(pmdMeshInfoPtr->skinList[i].Name),
							sizeof(pmdMeshInfoPtr->skinList[i].Name)
							);
					pmxMeshInfoPtr->morphList[i].NameEng
						= pmxMeshInfoPtr->morphList[i].Name;

					//
					pmxMeshInfoPtr->morphList[i].ControlPanel = pmdMeshInfoPtr->skinList[i].SkinType;
					//
					pmxMeshInfoPtr->morphList[i].Type = 1;//���_�Œ�
					//
					pmxMeshInfoPtr->morphList[i].DataNum = pmdMeshInfoPtr->skinList[i].VertexCount;

					switch (pmxMeshInfoPtr->morphList[i].Type)
					{
					case 1:	// ���_
						PmxSkinNum++;
						pmxMeshInfoPtr->morphList[i].Vertex.AddZeroed(pmxMeshInfoPtr->morphList[i].DataNum);

						for (j = 0; j < pmxMeshInfoPtr->morphList[i].DataNum; j++)
						{
							pmxMeshInfoPtr->morphList[i].Vertex[j].Index =
								pmdMeshInfoPtr->skinList[i].Vertex[j].TargetVertexIndex;
							//
							memcopySize = sizeof(pmxMeshInfoPtr->morphList[i].Vertex[j].Offset);
							FMemory::Memcpy(&pmxMeshInfoPtr->morphList[i].Vertex[j].Offset, 
								pmdMeshInfoPtr->skinList[i].Vertex[j].Position, memcopySize);
						}
						break;
					default:
						//un support ppmd
						break;
					}
				}
				UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning, TEXT("PMX convert [MorphList] Complete"));
			}
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning, TEXT("PMX convert [bonelList] Complete"));
		}
		return true;
	}


}