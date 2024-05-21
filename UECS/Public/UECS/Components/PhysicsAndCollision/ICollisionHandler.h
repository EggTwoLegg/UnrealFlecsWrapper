#pragma once

struct FNarrowPhaseEntityContact;

namespace flecs
{
	struct entity;
}

struct ICollisionHandler
{
	virtual void Handle(const FNarrowPhaseEntityContact& Contact, const flecs::entity& OtherEntity) = 0;

	virtual ~ICollisionHandler() = default;
};

struct FCollisionHandler
{
	TSharedPtr<ICollisionHandler> Handler { nullptr };
};