#include "UECS/Systems/SystemStaticMeshDraw.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "UECS/SystemTasks.h"
#include "UECS/Components/BaseComponents.h"

void SystemStaticMeshDraw::initialize(UWorldSubsystem* inOwner, flecs::world* inEcsWorld)
{
	UnrealEcsSystem::initialize(inOwner, inEcsWorld);
	
	queryISMOnly = inEcsWorld->query_builder<FInstancedStaticMesh>()
		.term<FInstancedStaticMesh>().inout().self()
		.build();
	
	queryISMAndTransforms = inEcsWorld->query_builder<const FTransformComponent, FInstancedStaticMesh>()
		.term<FTransformComponent>().in().self() // don't inherit the transform component.
	    .term<FInstancedStaticMesh>()
		.instanced()
		.build();
}

void SystemStaticMeshDraw::schedule(UnrealEcsSystemScheduler* inScheduler)
{
	SystemTaskChainBuilder builder("StaticMeshDraw", 1500, inScheduler);
	builder.AddDependency("PackTransforms");
	// builder.AddDependency("Movement");
	// builder.AddDependency("FollowPath");
	
	// Task: prepare render data
	{
		TaskDependencies deps;
		deps.AddWrite<FInstancedStaticMesh>();

		builder.AddTask(deps, [this](flecs::world& world)
		{
			queryISMAndTransforms.iter([&](const flecs::iter& iterator, const FTransformComponent* transform, FInstancedStaticMesh* ism)
			{
				if(iterator.is_self(1) && !iterator.is_self(2))
				{
					const auto numMatched = iterator.count();
					
					ism->addedTransforms.Reset();
					ism->addedTransforms.Reserve(numMatched);
					ism->updatedTransforms.Reset();
					ism->updatedTransforms.Reserve(numMatched);
					ism->removedIndices.Reset();
					ism->removedIndices.Reserve(numMatched);
					ism->numInstances = numMatched;
				}
			});
		}, ESysTaskType::TaskGraph);
	}

	// Task: Update the ism data map and add collision data to entities that have collisions.
	{
		TaskDependencies deps;
		deps.AddWrite<FInstancedStaticMesh>();
		deps.AddRead<FTransformComponent>();

		builder.AddTask(deps, [=](flecs::world& world)
		{
			queryISMAndTransforms.iter([&](const flecs::iter& iterator, const FTransformComponent* transform, FInstancedStaticMesh* ism)
			{
				if(iterator.is_self(1) && !iterator.is_self(2))
				{
					for(const auto idx : iterator)
					{
						UInstancedStaticMeshComponent* ismComp = ism->ismComp;
						const FTransform& renderTransform = transform[idx].value;
						
						if(ismComp->GetInstanceCount() < idx + 1)
						{
							ism->addedTransforms.Add(renderTransform);
						}
						else
						{
							ism->updatedTransforms.Add(renderTransform);
 						}

						const auto findOverlap = ism->entityOverlaps.Find(idx);
						if(nullptr != findOverlap)
						{
							world.defer_begin();
							iterator.entity(idx).add<FOverlapComponent>();
							world.defer_end();
						}
					}

					ism->entityOverlaps.Reset();
				}
			});
		}, ESysTaskType::TaskGraph);
	}

	// Task: Update the unreal ism component to reflect the changes.
	{
		TaskDependencies deps;
		deps.AddWrite<FInstancedStaticMesh>();
	
		builder.AddTask(deps, [=](flecs::world& world)
		{
			queryISMOnly.iter([&](const flecs::iter& iterator, FInstancedStaticMesh* ism)
			{
				if(iterator.is_self(1))
				{
					UInstancedStaticMeshComponent* ismComp = ism->ismComp;
					ismComp->AddInstances(ism->addedTransforms, false, true);
					ismComp->BatchUpdateInstancesTransforms(0, ism->updatedTransforms, true, false, false);
						
					const int32 instanceCount = ismComp->GetInstanceCount();
					while(ism->numInstances < instanceCount)
					{
						ism->removedIndices.Add(ism->numInstances);
						++ism->numInstances;
					}
					
					if(ism->removedIndices.Num() > 0) { ismComp->RemoveInstances(ism->removedIndices); }
					
					ismComp->MarkRenderStateDirty();
				}
			});
		}, ESysTaskType::GameThread);
	}

	inScheduler->AddSystemTaskChain(builder.FinishGraph());
}
