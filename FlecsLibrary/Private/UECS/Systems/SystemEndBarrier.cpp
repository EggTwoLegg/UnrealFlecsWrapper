#include "UECS/Systems/SystemEndBarrier.h"

#include "UECS/SystemTasks.h"

void SystemEndBarrier::schedule(UnrealEcsSystemScheduler* inScheduler)
{
	SystemTaskChainBuilder builder("EndBarrier", INT32_MAX, inScheduler);

	builder.AddTask({}, [=](flecs::world& world)
	{
		
	}, ESysTaskType::SyncPoint);

	inScheduler->AddSystemTaskChain(builder.FinishGraph());
}