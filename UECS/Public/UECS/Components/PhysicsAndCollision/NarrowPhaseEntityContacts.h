#pragma once

#include "UECS/flecs.h"

struct FNarrowPhaseEntityContact
{
	FVector ContactPoint {};
	FVector ContactNormal {};
	flecs::entity EntityHit;
	float Time { 0.0f };
	bool bIsBlockingHit { false };

	bool operator==(const FNarrowPhaseEntityContact& Other) const
	{
		return EntityHit == Other.EntityHit;
	}
};

FORCEINLINE uint32 GetTypeHash(const FNarrowPhaseEntityContact& Contact) { return Contact.EntityHit.id(); }

struct FNarrowPhaseEntityContacts
{
	TSet<FNarrowPhaseEntityContact> Contacts {};
};
