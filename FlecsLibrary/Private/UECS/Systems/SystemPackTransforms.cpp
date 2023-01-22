#include "UECS/Systems/SystemPackTransforms.h"

#include "UECS/SystemTasks.h"
#include "UECS/Components/BaseComponents.h"

void SystemPackTransforms::initialize(UWorldSubsystem* inOwner, flecs::world* inEcsWorld)
{
	UnrealEcsSystem::initialize(inOwner, inEcsWorld);

	queryTransforms = inEcsWorld->query_builder<const FPosition, const FRotationComponent, const FScale, FTransformComponent>()
		.term<FPosition>().in()
		.term<FRotationComponent>().in()
		.term<FScale>().in()
		.term<FTransformComponent>().out()
		.build();
}

void SystemPackTransforms::schedule(UnrealEcsSystemScheduler* inScheduler)
{
	SystemTaskChainBuilder builder("PackTransforms", 1500, inScheduler);
	
	// Task: Pack transforms from the position, rotation, and scale components into the ism component.
	{
		TaskDependencies deps;
		deps.AddRead<FPosition>();
		deps.AddRead<FRotationComponent>();
		deps.AddRead<FScale>();
		deps.AddWrite<FTransformComponent>();
		
		builder.AddTask(deps, [this](flecs::world& world)
		{
			queryTransforms.iter([&](const flecs::iter& iterator, const FPosition* position, const FRotationComponent* rotation, const FScale* scale, FTransformComponent* transform)
			{
				for(const auto idx : iterator)
				{
					transform[idx].value.SetLocation(position[idx].value);
					transform[idx].value.SetRotation(rotation[idx].value);
					transform[idx].value.SetScale3D(scale[idx].value);
				}
			});
		}, ESysTaskType::TaskGraph);
	}

	inScheduler->AddSystemTaskChain(builder.FinishGraph());
}
