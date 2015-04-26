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

UCLASS(hidecategories = Object)
class UMMDExtend : public UObject
{
	GENERATED_UCLASS_BODY()

public:


	UPROPERTY(EditAnywhere, Category = UMMDExtend)
	FString ModelName;
private:

};