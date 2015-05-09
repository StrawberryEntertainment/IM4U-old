// Copyright 2015 BlackMa9. All Rights Reserved.

#include "IM4UPrivatePCH.h"

#include "MMDExtendFactory.h"
#include "MMDExtend.h"

UMMDExtendFactory::UMMDExtendFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMMDExtend::StaticClass();
	bCreateNew = true; //for editor create flag
}
bool UMMDExtendFactory::DoesSupportClass(UClass* Class)
{
	return (Class == UMMDExtend::StaticClass());
}
UClass* UMMDExtendFactory::ResolveSupportedClass()
{
	return UMMDExtend::StaticClass();
}

UObject* UMMDExtendFactory::FactoryCreateNew(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn
	)
{
	UMMDExtend* NewAsset =
		CastChecked<UMMDExtend>(StaticConstructObject(InClass, InParent, InName, Flags));
	return NewAsset;
}
