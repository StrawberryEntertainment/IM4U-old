
#include "IM4UPrivatePCH.h"
#include "PmxImporter.h"

namespace MMD4UE4
{

	DEFINE_LOG_CATEGORY(LogMMD4UE4_PmxMeshInfo)

	PmxMeshInfo::PmxMeshInfo()
	{
	}


	PmxMeshInfo::~PmxMeshInfo()
	{
	}


	FVector PmxMeshInfo::ConvertVectorAsixToUE4FromMMD(FVector vec)
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



	bool PmxMeshInfo::PMXLoaderBinary(
		const uint8 *& Buffer,
		const uint8 * BufferEnd
		)
	{
		////////////////////////////////////////////

		PMXEncodeType pmxEncodeType = PMXEncodeType_ERROR;
		uint32 memcopySize = 0;
		float modelScale = 10.0f;

		FMemory::Memcpy(this->magic, Buffer, sizeof(this->magic));
		if (this->magic[0] == 'P' && this->magic[1] == 'M' && this->magic[2] == 'X')
		{
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning, TEXT("PMX Import START /Correct Magic[PMX]"));
		}
		else
		{
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Error, TEXT("PMX Import FALSE/Return /UnCorrect Magic[PMX]"));
			return false;
		}
		Buffer += sizeof(this->magic);

		FMemory::Memcpy(&this->formatVer, Buffer, sizeof(this->formatVer));
		Buffer += sizeof(this->formatVer);

		Buffer += sizeof(BYTE);
		/*
		�o�C�g�� - byte
		[0] - �G���R�[�h����  | 0:UTF16 1:UTF8
		[1] - �ǉ�UV�� 	| 0�`4 �ڍׂ͒��_�Q��

		[2] - ���_Index�T�C�Y | 1,2,4 �̂����ꂩ
		[3] - �e�N�X�`��Index�T�C�Y | 1,2,4 �̂����ꂩ
		[4] - �ގ�Index�T�C�Y | 1,2,4 �̂����ꂩ
		[5] - �{�[��Index�T�C�Y | 1,2,4 �̂����ꂩ
		[6] - ���[�tIndex�T�C�Y | 1,2,4 �̂����ꂩ
		[7] - ����Index�T�C�Y | 1,2,4 �̂����ꂩ
		*/
		FMemory::Memcpy(&this->baseHeader, Buffer, sizeof(this->baseHeader));
		//NewMyAsset->MyStruct.ModelName.Append((char*)BufferPMXHeaderPtr__Impl.byteDate);
		Buffer += sizeof(this->baseHeader);

		if (0 == this->baseHeader.EncodeType)
		{
			pmxEncodeType = PMXEncodeType_UTF16LE;
		}
		else if (1 == this->baseHeader.EncodeType)
		{
			pmxEncodeType = PMXEncodeType_UTF8;
		}
		else
		{
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Error, TEXT("PMX Import FALSE/Return /UnCorrect EncodeType"));
			return false;
		}
		this->modelNameJP		= PMXTexBufferToFString(&Buffer, pmxEncodeType);
		this->modelNameEng		= PMXTexBufferToFString(&Buffer, pmxEncodeType);
		this->modelCommentJP	= PMXTexBufferToFString(&Buffer, pmxEncodeType);
		this->modelCommentEng = PMXTexBufferToFString(&Buffer, pmxEncodeType);

		modelNameJP 
			= modelNameJP.Replace(TEXT("."), TEXT("_"));// [.] is broken filepath for ue4 
		////////////////////////////////////////////
		{
			//���v
			uint32 statics_bdef1 = 0;
			uint32 statics_bdef2 = 0;
			uint32 statics_bdef4 = 0;
			uint32 statics_sdef = 0;

			uint32 PmxVertexNum = 0;
			memcopySize = sizeof(PmxVertexNum);
			FMemory::Memcpy(&PmxVertexNum, Buffer, memcopySize);
			Buffer += memcopySize;

			vertexList.AddZeroed(PmxVertexNum);
			for (uint32 VertexIndex = 0; VertexIndex < PmxVertexNum; ++VertexIndex)
			{
				PMX_VERTEX & pmxVertexPtr = vertexList[VertexIndex];
				///
				//�ʒu(x,y,z)
				memcopySize = sizeof(pmxVertexPtr.Position);
				FMemory::Memcpy(&pmxVertexPtr.Position, Buffer, memcopySize);
				pmxVertexPtr.Position = ConvertVectorAsixToUE4FromMMD(pmxVertexPtr.Position)*modelScale;
				Buffer += memcopySize;
				//�@��(x,y,z)
				memcopySize = sizeof(pmxVertexPtr.Normal);
				FMemory::Memcpy(&pmxVertexPtr.Normal, Buffer, memcopySize);
				pmxVertexPtr.Normal = ConvertVectorAsixToUE4FromMMD(pmxVertexPtr.Normal);
				Buffer += memcopySize;
				//UV(u,v)
				memcopySize = sizeof(pmxVertexPtr.UV);
				FMemory::Memcpy(&pmxVertexPtr.UV, Buffer, memcopySize);
				Buffer += memcopySize;
				/*
				float tempUV = pmxVertexPtr.UV.X;//UE4���W�n���]
				pmxVertexPtr.UV.X = 1 - pmxVertexPtr.UV.Y;
				pmxVertexPtr.UV.Y = 1 - tempUV;
				*/
				//�ǉ�UV(x,y,z,w)  PMX�w�b�_�̒ǉ�UV���ɂ��	n:�ǉ�UV�� 0�`4
				for (int ExUVNum = 0; ExUVNum < this->baseHeader.UVNum; ++ExUVNum)
				{
					//
					memcopySize = sizeof(pmxVertexPtr.AddUV[ExUVNum]);
					FMemory::Memcpy(&pmxVertexPtr.AddUV[ExUVNum], Buffer, memcopySize);
					Buffer += memcopySize;
				}
				// �E�F�C�g�ό`���� 0:BDEF1 1:BDEF2 2:BDEF4 3:SDEF
				memcopySize = sizeof(pmxVertexPtr.WeightType);
				FMemory::Memcpy(&pmxVertexPtr.WeightType, Buffer, memcopySize);
				Buffer += memcopySize;
				//
				if (pmxVertexPtr.WeightType == 0)
				{
					statics_bdef1++;
					//BDEF1
					pmxVertexPtr.BoneIndex[0] = PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
					pmxVertexPtr.BoneIndex[0] ++;
					pmxVertexPtr.BoneWeight[0] = 1.0f;
				}
				else if(pmxVertexPtr.WeightType == 1)
				{
					statics_bdef2++;
					//BDEF2
					pmxVertexPtr.BoneIndex[0] = PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
					pmxVertexPtr.BoneIndex[0] ++;
					pmxVertexPtr.BoneIndex[1] = PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
					pmxVertexPtr.BoneIndex[1] ++;
					//
					memcopySize = sizeof(pmxVertexPtr.BoneWeight[0]);
					FMemory::Memcpy(&pmxVertexPtr.BoneWeight[0], Buffer, memcopySize);
					Buffer += memcopySize;
					//
					pmxVertexPtr.BoneWeight[1] = 1.0f - pmxVertexPtr.BoneWeight[0];
				}
				else if(pmxVertexPtr.WeightType == 2)
				{
					statics_bdef4++;
					//BDEF4
					pmxVertexPtr.BoneIndex[0] = PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
					pmxVertexPtr.BoneIndex[0] ++;
					pmxVertexPtr.BoneIndex[1] = PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
					pmxVertexPtr.BoneIndex[1] ++;
					pmxVertexPtr.BoneIndex[2] = PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
					pmxVertexPtr.BoneIndex[2] ++;
					pmxVertexPtr.BoneIndex[3] = PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
					pmxVertexPtr.BoneIndex[3] ++;
					for (int bw = 0; bw < 4; ++bw)
					{
						//
						memcopySize = sizeof(pmxVertexPtr.BoneWeight[bw]);
						FMemory::Memcpy(&pmxVertexPtr.BoneWeight[bw], Buffer, memcopySize);
						Buffer += memcopySize;
					}
				}
				else if(pmxVertexPtr.WeightType == 3)
				{
					statics_sdef++;
					//SDEF
					pmxVertexPtr.BoneIndex[0] = PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
					pmxVertexPtr.BoneIndex[0] ++;
					pmxVertexPtr.BoneIndex[1] = PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
					pmxVertexPtr.BoneIndex[1] ++;
					//
					memcopySize = sizeof(pmxVertexPtr.BoneWeight[0]);
					FMemory::Memcpy(&pmxVertexPtr.BoneWeight[0], Buffer, memcopySize);
					Buffer += memcopySize;
					pmxVertexPtr.BoneWeight[1] = 1.0f - pmxVertexPtr.BoneWeight[0];

					for (int bw = 0; bw < 1; ++bw)
					{
						//
						memcopySize = sizeof(pmxVertexPtr.SDEF_C);
						FMemory::Memcpy(&pmxVertexPtr.SDEF_C, Buffer, memcopySize);
						Buffer += memcopySize;
					}
					for (int bw = 0; bw < 1; ++bw)
					{
						//
						memcopySize = sizeof(pmxVertexPtr.SDEF_R0);
						FMemory::Memcpy(&pmxVertexPtr.SDEF_R0, Buffer, memcopySize);
						Buffer += memcopySize;
					}
					for (int bw = 0; bw < 1; ++bw)
					{
						//
						memcopySize = sizeof(pmxVertexPtr.SDEF_R1);
						FMemory::Memcpy(&pmxVertexPtr.SDEF_R1, Buffer, memcopySize);
						Buffer += memcopySize;
					}
				}
				else
				{
					UE_LOG(LogMMD4UE4_PmxMeshInfo, Error, TEXT("PMX Import FALSE/Return /UnCorrect EncodeType"));
				}
				//�G�b�W�{��  �ގ��̃G�b�W�T�C�Y�ɑ΂��Ă̔{���l
				memcopySize = sizeof(pmxVertexPtr.ToonEdgeScale);
				FMemory::Memcpy(&pmxVertexPtr.ToonEdgeScale, Buffer, memcopySize);
				Buffer += memcopySize;
			}
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning,
				TEXT("PMX Import [Vertex:: statics bone type, bdef1 = %u] Complete"), statics_bdef1);
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning,
				TEXT("PMX Import [Vertex:: statics bone type, bdef1 = %u] Complete"), statics_bdef2);
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning,
				TEXT("PMX Import [Vertex:: statics bone type, bdef1 = %u] Complete"), statics_bdef4);
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning,
				TEXT("PMX Import [Vertex:: statics bone type, bdef1 = %u] Complete"), statics_sdef);
		}
		UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning, TEXT("PMX Import [Vertex] Complete"));
		////////////////////////////////////////////
		{
			/*
			����

			n : ���_Index�T�C�Y     | ���_�̎Q��Index

			��3�_(3���_Index)��1��
			�ގ����̖ʐ��͍ގ����̖�(���_)���ŊǗ� (��PMD����)
			*/
			uint32 PmxFaceNum = 0;
			memcopySize = sizeof(PmxFaceNum);
			FMemory::Memcpy(&PmxFaceNum, Buffer, memcopySize);
			Buffer += memcopySize;

			PmxFaceNum /= 3;

			faseList.AddZeroed(PmxFaceNum);
			for (uint32 FaceIndex = 0; FaceIndex < PmxFaceNum; ++FaceIndex)
			{
				PMX_FACE & pmxFaseListPtr = faseList[FaceIndex];
				//
				for (int SubNum = 0; SubNum < 3; ++SubNum)
				{
					pmxFaseListPtr.VertexIndex[SubNum] 
						= PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.VertexIndexSize);
				}
			}
		}
		UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning, TEXT("PMX Import [FaceList] Complete"));
		////////////////////////////////////////////
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
		{
			// �}�e���A���̐����擾
			uint32 PmxMaterialNum = 0;
			//
			memcopySize = sizeof(PmxMaterialNum);
			FMemory::Memcpy(&PmxMaterialNum, Buffer, memcopySize);
			Buffer += memcopySize;

			// �}�e���A���f�[�^���i�[���郁�����̈�̊m��
			materialList.AddZeroed(PmxMaterialNum);

			// �}�e���A���̓ǂ݂���
			for (uint32 i = 0; i < PmxMaterialNum; i++)
			{
				// �ގ����̎擾
				materialList[i].Name = PMXTexBufferToFString(&Buffer, pmxEncodeType);
				materialList[i].NameEng = PMXTexBufferToFString(&Buffer, pmxEncodeType);

				//Diffuse (R,G,B,A)
				memcopySize = sizeof(materialList[i].Diffuse);
				FMemory::Memcpy(materialList[i].Diffuse, Buffer, memcopySize);
				Buffer += memcopySize;
				//Specular (R,G,B)
				memcopySize = sizeof(materialList[i].Specular);
				FMemory::Memcpy(materialList[i].Specular, Buffer, memcopySize);
				Buffer += memcopySize;
				//Specular�W��
				memcopySize = sizeof(materialList[i].SpecularPower);
				FMemory::Memcpy(&materialList[i].SpecularPower, Buffer, memcopySize);
				Buffer += memcopySize;
				//Ambient (R,G,B)
				memcopySize = sizeof(materialList[i].Ambient);
				FMemory::Memcpy(materialList[i].Ambient, Buffer, memcopySize);
				Buffer += memcopySize;

				/*
				�`��t���O(8bit) - �ebit 0:OFF 1:ON
				0x01:���ʕ`��, 0x02:�n�ʉe, 0x04:�Z���t�V���h�E�}�b�v�ւ̕`��, 0x08:�Z���t�V���h�E�̕`��,
				0x10:�G�b�W�`��
				*/
				uint8 tempByte = 0;
				memcopySize = sizeof(tempByte);
				FMemory::Memcpy(&tempByte, Buffer, memcopySize);
				Buffer += memcopySize;
				materialList[i].CullingOff = (tempByte & 0x01) ? 1 : 0;
				materialList[i].GroundShadow = (tempByte & 0x02) ? 1 : 0;
				materialList[i].SelfShadowMap = (tempByte & 0x04) ? 1 : 0;
				materialList[i].SelfShadowDraw = (tempByte & 0x08) ? 1 : 0;
				materialList[i].EdgeDraw = (tempByte & 0x10) ? 1 : 0;

				//�G�b�W�F (R,G,B,A)
				memcopySize = sizeof(materialList[i].EdgeColor);
				FMemory::Memcpy(materialList[i].EdgeColor, Buffer, memcopySize);
				Buffer += memcopySize;

				//�G�b�W�T�C�Y
				memcopySize = sizeof(materialList[i].EdgeSize);
				FMemory::Memcpy(&materialList[i].EdgeSize, Buffer, memcopySize);
				Buffer += memcopySize;
				//�ʏ�e�N�X�`��, �e�N�X�`���e�[�u���̎Q��Index
				materialList[i].TextureIndex
					= PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.TextureIndexSize);
				//�X�t�B�A�e�N�X�`��, �e�N�X�`���e�[�u���̎Q��Index  ���e�N�X�`���g���q�̐����Ȃ�
				materialList[i].SphereTextureIndex 
					= PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.TextureIndexSize);
				//�X�t�B�A���[�h 0:���� 1:��Z(sph) 2:���Z(spa) 
				//3:�T�u�e�N�X�`��(�ǉ�UV1��x,y��UV�Q�Ƃ��Ēʏ�e�N�X�`���`����s��)
				memcopySize = sizeof(materialList[i].SphereMode);
				FMemory::Memcpy(&materialList[i].SphereMode, Buffer, memcopySize);
				Buffer += memcopySize;
				//���LToon�t���O 0:�p���l�͌�Toon 1 : �p���l�͋��LToon
				memcopySize = sizeof(materialList[i].ToonFlag);
				FMemory::Memcpy(&materialList[i].ToonFlag, Buffer, memcopySize);
				Buffer += memcopySize;

				if (materialList[i].ToonFlag == 0)
				{//Toon�e�N�X�`��, �e�N�X�`���e�[�u���̎Q��Index
					materialList[i].ToonTextureIndex 
						= PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.TextureIndexSize);
				}
				else
				{//���LToon�e�N�X�`��[0�`9] -> ���ꂼ�� toon01.bmp�`toon10.bmp �ɑΉ�
					memcopySize = sizeof(BYTE);
					FMemory::Memcpy(&materialList[i].ToonTextureIndex, Buffer, memcopySize);
					Buffer += memcopySize;
				}

				// �����̓X�L�b�v
				PMXTexBufferToFString(&Buffer, pmxEncodeType);
				//�ގ��ɑΉ������(���_)�� (�K��3�̔{���ɂȂ�)
				memcopySize = sizeof(materialList[i].MaterialFaceNum);
				FMemory::Memcpy(&materialList[i].MaterialFaceNum, Buffer, memcopySize);
				Buffer += memcopySize;
			}
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning, TEXT("PMX Import [materialList] Complete"));
		}
		////////////////////////////////////////////
#if 1
		{
			// ���ׂĂ̐eBone��UE4�����ɒǉ�
			// �{�[���f�[�^���i�[���郁�����̈�̊m��
			boneList.AddZeroed(1);

			// �{�[���̐����擾
			uint32 PmxBoneNum = 0;
			uint32 offsetBoneIndx = 1;

			// �{�[�����̎擾
			{
				boneList[PmxBoneNum].Name = "AllTopRootBone";
				boneList[PmxBoneNum].NameEng = "AllTopRootBone";
				boneList[PmxBoneNum].Position = FVector(0);
				boneList[PmxBoneNum].ParentBoneIndex = INDEX_NONE;
			}
			//
			memcopySize = sizeof(PmxBoneNum);
			FMemory::Memcpy(&PmxBoneNum, Buffer, memcopySize);
			Buffer += memcopySize;

			// �{�[���f�[�^���i�[���郁�����̈�̊m��
			boneList.AddZeroed(PmxBoneNum);

			// �{�[�����̎擾
			uint32 PmxIKNum = 0;
			for (uint32 i = 1; i < PmxBoneNum + offsetBoneIndx; i++)
			{

				boneList[i].Name
					= PMXTexBufferToFString(&Buffer, pmxEncodeType);
				boneList[i].NameEng
					= PMXTexBufferToFString(&Buffer, pmxEncodeType);
				//
				memcopySize = sizeof(boneList[i].Position);
				FMemory::Memcpy(&boneList[i].Position, Buffer, memcopySize);
				boneList[i].Position = ConvertVectorAsixToUE4FromMMD(boneList[i].Position) *modelScale;
				Buffer += memcopySize;

				boneList[i].ParentBoneIndex
					= PMXExtendBufferSizeToInt32(&Buffer, this->baseHeader.BoneIndexSize) + offsetBoneIndx;
				//
				memcopySize = sizeof(boneList[i].TransformLayer);
				FMemory::Memcpy(&boneList[i].TransformLayer, Buffer, memcopySize);
				Buffer += memcopySize;

				WORD Flag;
				//
				memcopySize = sizeof(Flag);
				FMemory::Memcpy(&Flag, Buffer, memcopySize);
				Buffer += memcopySize;

				boneList[i].Flag_LinkDest = (Flag & 0x0001) != 0 ? 1 : 0;
				boneList[i].Flag_EnableRot = (Flag & 0x0002) != 0 ? 1 : 0;
				boneList[i].Flag_EnableMov = (Flag & 0x0004) != 0 ? 1 : 0;
				boneList[i].Flag_Disp = (Flag & 0x0008) != 0 ? 1 : 0;
				boneList[i].Flag_EnableControl = (Flag & 0x0010) != 0 ? 1 : 0;
				boneList[i].Flag_IK = (Flag & 0x0020) != 0 ? 1 : 0;
				boneList[i].Flag_AddRot = (Flag & 0x0100) != 0 ? 1 : 0;
				boneList[i].Flag_AddMov = (Flag & 0x0200) != 0 ? 1 : 0;
				boneList[i].Flag_LockAxis = (Flag & 0x0400) != 0 ? 1 : 0;
				boneList[i].Flag_LocalAxis = (Flag & 0x0800) != 0 ? 1 : 0;
				boneList[i].Flag_AfterPhysicsTransform = (Flag & 0x1000) != 0 ? 1 : 0;
				boneList[i].Flag_OutParentTransform = (Flag & 0x2000) != 0 ? 1 : 0;

				if (boneList[i].Flag_LinkDest == 0)
				{
					//
					memcopySize = sizeof(boneList[i].OffsetPosition);
					FMemory::Memcpy(&boneList[i].OffsetPosition, Buffer, memcopySize);
					boneList[i].OffsetPosition = ConvertVectorAsixToUE4FromMMD(boneList[i].OffsetPosition) *modelScale;
					Buffer += memcopySize;
				}
				else
				{
					boneList[i].LinkBoneIndex
						= PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
				}

				if (boneList[i].Flag_AddRot == 1 || boneList[i].Flag_AddMov == 1)
				{
					boneList[i].AddParentBoneIndex
						= PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
					//
					memcopySize = sizeof(boneList[i].AddRatio);
					FMemory::Memcpy(&boneList[i].AddRatio, Buffer, memcopySize);
					Buffer += memcopySize;
				}

				if (boneList[i].Flag_LockAxis == 1)
				{
					//
					memcopySize = sizeof(boneList[i].LockAxisVector);
					FMemory::Memcpy(&boneList[i].LockAxisVector, Buffer, memcopySize);
					Buffer += memcopySize;
				}

				if (boneList[i].Flag_LocalAxis == 1)
				{

					//
					memcopySize = sizeof(boneList[i].LocalAxisXVector);
					FMemory::Memcpy(&boneList[i].LocalAxisXVector, Buffer, memcopySize);
					Buffer += memcopySize;

					//
					memcopySize = sizeof(boneList[i].LocalAxisZVector);
					FMemory::Memcpy(&boneList[i].LocalAxisZVector, Buffer, memcopySize);
					Buffer += memcopySize;
				}

				if (boneList[i].Flag_OutParentTransform == 1)
				{
					//
					memcopySize = sizeof(boneList[i].OutParentTransformKey);
					FMemory::Memcpy(&boneList[i].OutParentTransformKey, Buffer, memcopySize);
					Buffer += memcopySize;
				}

				if (boneList[i].Flag_IK == 1)
				{
					PmxIKNum++;

					boneList[i].IKInfo.TargetBoneIndex
						= PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
					//
					memcopySize = sizeof(boneList[i].IKInfo.LoopNum);
					FMemory::Memcpy(&boneList[i].IKInfo.LoopNum, Buffer, memcopySize);
					Buffer += memcopySize;

					//
					memcopySize = sizeof(boneList[i].IKInfo.RotLimit);
					FMemory::Memcpy(&boneList[i].IKInfo.RotLimit, Buffer, memcopySize);
					Buffer += memcopySize;

					//
					memcopySize = sizeof(boneList[i].IKInfo.LinkNum);
					FMemory::Memcpy(&boneList[i].IKInfo.LinkNum, Buffer, memcopySize);
					Buffer += memcopySize;
					if (boneList[i].IKInfo.LinkNum >= PMX_MAX_IKLINKNUM)
					{
						return false;
					}

					for (int32 j = 0; j < boneList[i].IKInfo.LinkNum; j++)
					{
						boneList[i].IKInfo.Link[j].BoneIndex
							= PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
						//
						memcopySize = sizeof(boneList[i].IKInfo.Link[j].RotLockFlag);
						FMemory::Memcpy(&boneList[i].IKInfo.Link[j].RotLockFlag, Buffer, memcopySize);
						Buffer += memcopySize;

						if (boneList[i].IKInfo.Link[j].RotLockFlag == 1)
						{
							//
							memcopySize = sizeof(boneList[i].IKInfo.Link[j].RotLockMin);
							FMemory::Memcpy(&boneList[i].IKInfo.Link[j].RotLockMin[0], Buffer, memcopySize);
							Buffer += memcopySize;
							//
							memcopySize = sizeof(boneList[i].IKInfo.Link[j].RotLockMax);
							FMemory::Memcpy(&boneList[i].IKInfo.Link[j].RotLockMax[0], Buffer, memcopySize);
							Buffer += memcopySize;
						}
					}
				}
			}
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning, TEXT("PMX Import [BoneList] Complete"));
		}
#endif
#if 1
		{
			int32 i, j;
			// ���[�t���̐����擾
			int32 PmxMorphNum = 0;
			//
			memcopySize = sizeof(PmxMorphNum);
			FMemory::Memcpy(&PmxMorphNum, Buffer, memcopySize);
			Buffer += memcopySize;

			// ���[�t�f�[�^���i�[���郁�����̈�̊m��
			morphList.AddZeroed(PmxMorphNum);

			// ���[�t���̓ǂݍ���
			int32 PmxSkinNum = 0;
			for (i = 0; i < PmxMorphNum; i++)
			{
				morphList[i].Name
					= PMXTexBufferToFString(&Buffer, pmxEncodeType);
				morphList[i].NameEng
					= PMXTexBufferToFString(&Buffer, pmxEncodeType);

				//
				memcopySize = sizeof(morphList[i].ControlPanel);
				FMemory::Memcpy(&morphList[i].ControlPanel, Buffer, memcopySize);
				Buffer += memcopySize;
				//
				memcopySize = sizeof(morphList[i].Type);
				FMemory::Memcpy(&morphList[i].Type, Buffer, memcopySize);
				Buffer += memcopySize;
				//
				memcopySize = sizeof(morphList[i].DataNum);
				FMemory::Memcpy(&morphList[i].DataNum, Buffer, memcopySize);
				Buffer += memcopySize;

				switch (morphList[i].Type)
				{
				case 0:	// �O���[�v���[�t
					morphList[i].Group.AddZeroed(morphList[i].DataNum);

					for (j = 0; j < morphList[i].DataNum; j++)
					{
						morphList[i].Group[j].Index =
							PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.MorphIndexSize);
						//
						memcopySize = sizeof(morphList[i].Group[j].Ratio);
						FMemory::Memcpy(&morphList[i].Group[j].Ratio, Buffer, memcopySize);
						Buffer += memcopySize;
					}
					break;

				case 1:	// ���_
					PmxSkinNum++;
					morphList[i].Vertex.AddZeroed(morphList[i].DataNum);

					for (j = 0; j < morphList[i].DataNum; j++)
					{
						morphList[i].Vertex[j].Index =
							PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.VertexIndexSize);
						//
						memcopySize = sizeof(morphList[i].Vertex[j].Offset);
						FMemory::Memcpy(&morphList[i].Vertex[j].Offset, Buffer, memcopySize);
						morphList[i].Vertex[j].Offset = ConvertVectorAsixToUE4FromMMD(morphList[i].Vertex[j].Offset)*modelScale;
						Buffer += memcopySize;
					}
					break;

				case 2:	// �{�[�����[�t
					morphList[i].Bone.AddZeroed(morphList[i].DataNum);

					for (j = 0; j < morphList[i].DataNum; j++)
					{
						morphList[i].Bone[j].Index =
							PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.BoneIndexSize);
						//
						memcopySize = sizeof(morphList[i].Bone[j].Offset);
						FMemory::Memcpy(&morphList[i].Bone[j].Offset, Buffer, memcopySize);
						morphList[i].Bone[j].Offset = ConvertVectorAsixToUE4FromMMD(morphList[i].Bone[j].Offset)*modelScale;
						Buffer += memcopySize;
						//
						memcopySize = sizeof(morphList[i].Bone[j].Quat);
						FMemory::Memcpy(&morphList[i].Bone[j].Quat, Buffer, memcopySize);
						Buffer += memcopySize;
					}
					break;

				case 3:	// UV���[�t
				case 4:	// �ǉ�UV1���[�t
				case 5:	// �ǉ�UV2���[�t
				case 6:	// �ǉ�UV3���[�t
				case 7:	// �ǉ�UV4���[�t
					morphList[i].UV.AddZeroed(morphList[i].DataNum);

					for (j = 0; j < morphList[i].DataNum; j++)
					{
						morphList[i].UV[j].Index =
							PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.VertexIndexSize);
						//
						memcopySize = sizeof(morphList[i].UV[j].Offset);
						FMemory::Memcpy(&morphList[i].UV[j].Offset, Buffer, memcopySize);
						Buffer += memcopySize;
					}
					break;

				case 8:	// �ގ����[�t
					morphList[i].Material.AddZeroed(morphList[i].DataNum);

					for (j = 0; j < morphList[i].DataNum; j++)
					{
						morphList[i].Material[j].Index =
							PMXExtendBufferSizeToUint32(&Buffer, this->baseHeader.MaterialIndexSize);
						//
						memcopySize = sizeof(morphList[i].Material[j].CalcType);
						FMemory::Memcpy(&morphList[i].Material[j].CalcType, Buffer, memcopySize);
						Buffer += memcopySize;

						//
						memcopySize = sizeof(morphList[i].Material[j].Diffuse);
						FMemory::Memcpy(&morphList[i].Material[j].Diffuse, Buffer, memcopySize);
						Buffer += memcopySize;
						//
						memcopySize = sizeof(morphList[i].Material[j].Specular);
						FMemory::Memcpy(&morphList[i].Material[j].Specular, Buffer, memcopySize);
						Buffer += memcopySize;
						//
						memcopySize = sizeof(morphList[i].Material[j].SpecularPower);
						FMemory::Memcpy(&morphList[i].Material[j].SpecularPower, Buffer, memcopySize);
						Buffer += memcopySize;
						//
						memcopySize = sizeof(morphList[i].Material[j].Ambient);
						FMemory::Memcpy(&morphList[i].Material[j].Ambient, Buffer, memcopySize);
						Buffer += memcopySize;

						//
						memcopySize = sizeof(morphList[i].Material[j].EdgeColor);
						FMemory::Memcpy(&morphList[i].Material[j].EdgeColor, Buffer, memcopySize);
						Buffer += memcopySize;

						//
						memcopySize = sizeof(morphList[i].Material[j].EdgeSize);
						FMemory::Memcpy(&morphList[i].Material[j].EdgeSize, Buffer, memcopySize);
						Buffer += memcopySize;

						//
						memcopySize = sizeof(morphList[i].Material[j].TextureScale);
						FMemory::Memcpy(&morphList[i].Material[j].TextureScale, Buffer, memcopySize);
						Buffer += memcopySize;

						//
						memcopySize = sizeof(morphList[i].Material[j].SphereTextureScale);
						FMemory::Memcpy(&morphList[i].Material[j].SphereTextureScale, Buffer, memcopySize);
						Buffer += memcopySize;
						//
						memcopySize = sizeof(morphList[i].Material[j].ToonTextureScale);
						FMemory::Memcpy(&morphList[i].Material[j].ToonTextureScale, Buffer, memcopySize);
						Buffer += memcopySize;

					}
					break;
				}
			}
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning, TEXT("PMX Import [MorphList] Complete"));
		}
#endif
		//////////////////////////////////////////////
		UE_LOG(LogMMD4UE4_PmxMeshInfo, Warning, TEXT("PMX Importer Class Complete"));
		return true;
	}

	//////////////////////////////////////
	// from PMD/PMX Binary Buffer To String @ TextBuf
	// 4 + n: TexBuf
	// buf : top string (top data)
	// encodeType : 0 utf-16, 1 utf-8
	//////////////////////////////////////
	FString PmxMeshInfo::PMXTexBufferToFString(const uint8 ** buffer, PMXEncodeType encodeType)
	{
		FString NewString;
		uint32 size = 0;
		//temp data 
		TArray<uint8> RawModData;

		if (encodeType == PMXEncodeType_UTF16LE)
		{
			FMemory::Memcpy(&size, *buffer, sizeof(uint32));
			*buffer += sizeof(uint32);
			RawModData.Empty(size);
			RawModData.AddUninitialized(size);
			FMemory::Memcpy(RawModData.GetData(), *buffer, RawModData.Num());
			RawModData.Add(0); RawModData.Add(0);
			NewString.Append((TCHAR*)RawModData.GetData());
			*buffer += size;
		}
		else
		{
			// this plugin  unsuported encodetype ( utf-8 etc.)
			// Error ...
			UE_LOG(LogMMD4UE4_PmxMeshInfo, Error, TEXT("PMX Encode Type : not UTF-16 LE , unload"));
		}
		return NewString;
	}

	/////////////////////////////////////
	//
	//////////////////////////////////////
	uint32 PmxMeshInfo::PMXExtendBufferSizeToUint32(
		const uint8 ** buffer,
		const uint8  blockSize
		)
	{
		uint32 retValue = 0;

		switch (blockSize)
		{
		case 1:
			retValue = (uint8)((*buffer)[0]);
			*buffer += blockSize;
			break;

		case 2:
			retValue = (uint16)(((uint16 *)*buffer)[0]);
			*buffer += blockSize;
			break;

		case 4:
			retValue = (uint32)((uint32 *)*buffer)[0];
			*buffer += blockSize;
			break;
		}

		return retValue;
	}
	int32 PmxMeshInfo::PMXExtendBufferSizeToInt32(
		const uint8 ** buffer,
		const uint8  blockSize
		)
	{
		int32 retValue = 0;

		switch (blockSize)
		{
		case 1:
			retValue = (int8)((*buffer)[0]);
			*buffer += blockSize;
			break;

		case 2:
			retValue = (int16)(((int16 *)*buffer)[0]);
			*buffer += blockSize;
			break;

		case 4:
			retValue = (int32)((int32 *)*buffer)[0];
			*buffer += blockSize;
			break;
		}

		return retValue;
	}
}