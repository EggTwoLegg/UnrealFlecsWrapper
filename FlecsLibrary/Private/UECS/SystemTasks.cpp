#include "UECS/SystemTasks.h"
#include <fstream>

DECLARE_CYCLE_STAT(TEXT("TaskSys: SyncPoint"), STAT_TS_SyncPoint, STATGROUP_ECS);
DECLARE_CYCLE_STAT(TEXT("TaskSys: GameTask"), STAT_TS_GameTask, STATGROUP_ECS);
DECLARE_CYCLE_STAT(TEXT("TaskSys: AsyncTask"), STAT_TS_AsyncTask, STATGROUP_ECS);
DECLARE_CYCLE_STAT(TEXT("TaskSys: TaskEnd1"), STAT_TS_End1, STATGROUP_ECS);
DECLARE_CYCLE_STAT(TEXT("TaskSys: TaskEnd2"), STAT_TS_End2, STATGROUP_ECS);
DECLARE_CYCLE_STAT(TEXT("TaskSys: WaitTime"), STAT_TS_Wait, STATGROUP_ECS);
DECLARE_CYCLE_STAT(TEXT("TaskSys: SyncLoop"), STAT_TS_SyncLoop, STATGROUP_ECS);

UnrealEcsSystemScheduler::UnrealEcsSystemScheduler(flecs::world* inEcsWorld, const bool inCacheSystemDependencies) : ecsWorld(inEcsWorld), totalTasks(0),
	threadSignal(nullptr),
	cacheSystemDependencies(inCacheSystemDependencies),
	didCacheSystemDependencies(false)
{
}

void UnrealEcsSystemScheduler::AddSystemTaskChain(SystemTaskChain* newGraph)
{
	sysTaskChains.Add(newGraph);
}

void UnrealEcsSystemScheduler::CacheSystemDependencies()
{
	// Sort system tasks by their sorting keys.
	sysTaskChains.Sort([](const SystemTaskChain& tskA, const SystemTaskChain& tskB)
	{
		return tskA.sortKey < tskB.sortKey;
	});
	
	auto FindIndexByDependencyName = [&](const FName& name) -> int
	{
		for (int idxTask = 0; idxTask < sysTaskChains.Num(); idxTask++)
		{
			if (sysTaskChains[idxTask]->name == name) return idxTask;
		}
		return 0;
	};

	// Reorder system tasks by their dependencies.
	for (int idxSysTask = 0; idxSysTask < sysTaskChains.Num() - 1; ++idxSysTask)
	{
		if (sysTaskChains[idxSysTask]->SystemDependencies.Num() > 0)
		{
			int idxMin = 0;
			
			// Get the minimum index this task can be at.
			for (const auto& depName : sysTaskChains[idxSysTask]->SystemDependencies)
			{
				idxMin = FMath::Max(idxMin, FindIndexByDependencyName(depName));
			}

			// If the task is below its dependencies, it needs to be placed after, and the span of tasks between
			// needs to be shuffled back.
			if (idxMin > idxSysTask)
			{
				//reshuffle the array
				SystemTaskChain* movechain = sysTaskChains[idxSysTask];

				//move all one down to cover the slot
				for (int idxMoveTask = idxSysTask; idxMoveTask <= idxMin; idxMoveTask++)
				{
					sysTaskChains[idxMoveTask] = sysTaskChains[idxMoveTask + 1];
				}

				sysTaskChains[idxMin] = movechain;
				--idxSysTask;
			}
		}
	}

	if(cacheSystemDependencies) { didCacheSystemDependencies = true; }
}

GraphTask* UnrealEcsSystemScheduler::BuildGraphTasks()
{
	//schedule all the taskchains into a proper task graph
	GraphTask* rootTask = NewGraphTask(nullptr);
	GraphTask* lastSyncTask = rootTask;
	TArray<GraphTask*> unconnectedTasks;

	for (int idxTask = 0; idxTask < sysTaskChains.Num(); idxTask++)
	{
		SystemTaskChain* taskChain = sysTaskChains[idxTask];

		int idxChain = 0;
		GraphTask* graphTask = NewGraphTask(taskChain->firstTask);
		graphTask->chainIndex = idxChain;
		lastSyncTask->AddSuccessor(graphTask);

		while (nullptr != graphTask)
		{
			idxChain++;

			// Synchronization point occurred, connect this graph task to it.
			if (graphTask->original->type == ESysTaskType::SyncPoint)
			{
				for (const auto unconnectedTask : unconnectedTasks)
				{
					unconnectedTask->AddSuccessor(graphTask);
				}
				unconnectedTasks.Empty();

				lastSyncTask = graphTask;
			}

			if (nullptr != graphTask->original->next)
			{
				GraphTask* newtask = NewGraphTask(graphTask->original->next);
				newtask->chainIndex = idxChain;
				graphTask->AddSuccessor(newtask);
				graphTask = newtask;
			}
			else
			{
				unconnectedTasks.Add(graphTask);
				break;
			}
		}
	}
	
	for (const auto allocatedGraphTask : AllocatedGraphTasks)
	{
		if (allocatedGraphTask->original && allocatedGraphTask->original->type == ESysTaskType::GameThread)
		{
			allocatedGraphTask->priorityWeight *= 2;
		}
	}

	// Setup successors for our new graph tasks.
	for (int idxTaskChain = 0; idxTaskChain < sysTaskChains.Num(); idxTaskChain++)
	{
		SystemTaskChain* taskChain = sysTaskChains[idxTaskChain];
		
		for (const auto& dep : taskChain->SystemDependencies)
		{
			//connect the first task of this to the last of dependencies

			GraphTask* chainGraphTask = nullptr;
			for (const auto graphTask : AllocatedGraphTasks)
			{
				if (nullptr != graphTask && graphTask->original == taskChain->firstTask)
				{
					chainGraphTask = graphTask;
					break;
				}
			}

			GraphTask* deptask = nullptr;

			for (int j = 0; j < sysTaskChains.Num(); j++)
			{
				if (sysTaskChains[j]->name == dep)
				{
					SystemTaskChain* depchain = sysTaskChains[j];

					for (auto t : AllocatedGraphTasks)
					{
						if (t && t->original == depchain->lastTask)
						{
							deptask = t;
							break;
						}
					}

					if (deptask) break;
				}
			}

			if(nullptr == deptask) { continue; }
			
			bool bfound = false;

			for (auto successor : deptask->successors)
			{
				if (successor == chainGraphTask)
				{
					bfound = true;
					break;
				}
			}
			if (!bfound)
			{
				deptask->AddSuccessor(chainGraphTask);
			}
		}
	}
	
	return rootTask;
}

void UnrealEcsSystemScheduler::Run(flecs::world& inEcsWorld)
{
	ecsWorld = &inEcsWorld;
	
	if(!didCacheSystemDependencies) { CacheSystemDependencies(); }

	GraphTask* rootTask = BuildGraphTasks();

	if constexpr(false) // Set to true to run sync instead of parallel.
	{
		TArray<GraphTask*> pendingNonParallelTasks;
	
		for (auto successor : rootTask->successors)
		{
			--(successor->predecessorCount);
			if (successor->predecessorCount == 0)
			{
				pendingNonParallelTasks.Add(successor);
			}
		}
	
		while (pendingNonParallelTasks.Num() > 0)
		{
			for (const auto pendingTask : pendingNonParallelTasks)
			{
				pendingTask->original->function(inEcsWorld);
			}
	
			TArray<GraphTask*> newTasks;
	
			for (const auto pendingTask : pendingNonParallelTasks)
			{
				for (auto successor : pendingTask->successors)
				{
					--(successor->predecessorCount);
					if (successor->predecessorCount == 0)
					{
						newTasks.Add(successor);
					}
				}
			}
			
			pendingNonParallelTasks = newTasks;
		}
	}
	else
	{
		threadSignal = FPlatformProcess::GetSynchEventFromPool(false);

		totalTasks = AllocatedGraphTasks.Num();

		AsyncFinished(rootTask);

		int  curLoops  = 0;
		bool breakLoop = false;
		while (totalTasks > 0 && !breakLoop)
		{
			SCOPE_CYCLE_COUNTER(STAT_TS_SyncLoop);

			// Go over the gamethread tasks, first.
			GraphTask* gameThreadTask;
			while (gameThreadTasks.Dequeue(gameThreadTask))
			{
				{
					SCOPE_CYCLE_COUNTER(STAT_TS_GameTask);
					gameThreadTask->original->function(inEcsWorld);

					AsyncFinished(gameThreadTask);
				}
			}
			
			endmutex.Lock();

			// Now iterate over the tasks that were unable to be immediately enqueued.
			if (pendingTasks.Num() == 0)
			{
				SCOPE_CYCLE_COUNTER(STAT_TS_SyncPoint);

				GraphTask* localSyncTask = syncTask;

				while (nullptr != localSyncTask)
				{
					localSyncTask->original->function(inEcsWorld);
					syncTask = nullptr;

					AsyncFinished(localSyncTask);

					localSyncTask = nullptr;
					if (syncTask != nullptr) { localSyncTask = syncTask; }
				};
			}
			
			endmutex.Unlock();
			threadSignal->Wait(1); // Let other threads continue.
			++curLoops;
			
			breakLoop = curLoops > 100;
		}
	}
}

void UnrealEcsSystemScheduler::AsyncFinished(GraphTask* task)
{
	SCOPE_CYCLE_COUNTER(STAT_TS_End1);
	
	endmutex.Lock();

	--totalTasks;
	if (nullptr != task->original)
	{
		RemovePending(task);
	}
	
	endmutex.Unlock();
	threadSignal->Trigger(); // Let other threads continue.
	
	{
		endmutex.Lock();
		TArray<GraphTask*, TInlineAllocator<5>> executableTasks;

		// Add successors to the waiting tasks.
		for (auto successor : task->successors)
		{
			successor->predecessorCount--;
			if (successor->predecessorCount == 0)
			{
				waitingTasks.Add(successor);
			}
		}

		// Move waiting tasks to executable tasks.
		if (waitingTasks.Num() != 0)
		{
			TArray<int, TInlineAllocator<5>> indicesToRemove;

			for (int idxWaitingTask = 0; idxWaitingTask < waitingTasks.Num(); idxWaitingTask++)
			{
				if (CanExecute(waitingTasks[idxWaitingTask]))
				{
					GraphTask* tsk = waitingTasks[idxWaitingTask];
					executableTasks.Add(tsk);
					indicesToRemove.Add(idxWaitingTask);
				}
			}

			// Remove all waiting tasks that were promoted to executable tasks.
			for (const auto idxToRemove : indicesToRemove)
			{
				waitingTasks[idxToRemove] = nullptr;
			}
			waitingTasks.Remove(nullptr);
		}
		
		endmutex.Unlock();

		// Sort executable tasks by their priority weight.
		executableTasks.Sort([](const GraphTask& tA, const GraphTask& tB)
		{
			return tA.priorityWeight > tB.priorityWeight;
		});

		// Launch executable tasks.
		for (const auto executableTask : executableTasks)
		{
			LaunchTask(executableTask);
		}
	}
}

bool UnrealEcsSystemScheduler::LaunchTask(GraphTask* task)
{
	SCOPE_CYCLE_COUNTER(STAT_TS_End2);
	
	endmutex.Lock();
	const ESysTaskType tasktype = task->original->type;
	if (tasktype == ESysTaskType::SyncPoint)
	{
		syncTask = task;
		endmutex.Unlock();

		return true;
	}
	
	if (!CanExecute(task))
	{
		waitingTasks.Add(task);
		endmutex.Unlock();
		return false;
	}

	if (tasktype == ESysTaskType::TaskGraph || tasktype == ESysTaskType::ThreadPool)
	{
		const EAsyncExecution exec = tasktype == ESysTaskType::TaskGraph ? EAsyncExecution::TaskGraph : EAsyncExecution::ThreadPool;

		TFuture<void> future = Async(exec, [=]()
			{
				SCOPE_CYCLE_COUNTER(STAT_TS_AsyncTask);
				task->original->function(*ecsWorld);
				AsyncFinished(task);
			},

          [=]()
			{
			}
		);

		AddPending(task, &future);
	}
	else
	{
		AddPending(task, nullptr);
		gameThreadTasks.Enqueue(task);
	}

	endmutex.Unlock();
	
	return true;
}

void UnrealEcsSystemScheduler::RemovePending(const GraphTask* task)
{
	//remove from pending tasks
	for (int idxPendingTask = 0; idxPendingTask < pendingTasks.Num(); idxPendingTask++)
	{
		if (pendingTasks[idxPendingTask]->task == task)
		{
			pendingTasks.RemoveAt(idxPendingTask);
			break;
		}
	}
}

void UnrealEcsSystemScheduler::AddPending(GraphTask* task, TFuture<void>* future)
{
	const TSharedPtr<LaunchedTask> newTask = MakeShared<LaunchedTask>();

	newTask->task = task;
	if (future)
	{
		newTask->future = std::move(*future);
	}
	
	newTask->dependencies = task->original->deps;

	pendingTasks.Add(newTask);
}

bool UnrealEcsSystemScheduler::CanExecute(const GraphTask* task)
{
	for (const auto& pendingTask : pendingTasks)
	{
		if (pendingTask->dependencies.ConflictsWith(task->original->deps))
		{
			return false;
		}
	}
	return true;
}

SystemTask* UnrealEcsSystemScheduler::NewTask()
{
	SystemTask* task = new SystemTask();

	AllocatedTasks.Add(task);

	return task;
}

GraphTask* UnrealEcsSystemScheduler::NewGraphTask(SystemTask* originalTask)
{
	GraphTask* task = new GraphTask();
	task->original = originalTask;
	task->predecessorCount = 0;
	task->priorityWeight = 1;
	if (nullptr != originalTask && nullptr != originalTask->ownerGraph)
	{
		task->priorityWeight *= originalTask->ownerGraph->priority;
	}

	AllocatedGraphTasks.Add(task);

	return task;
}

SystemTaskChain* UnrealEcsSystemScheduler::NewTaskChain()
{
	SystemTaskChain* task = new SystemTaskChain();
	AllocatedChains.Add(task);
	return task;
}

UnrealEcsSystemScheduler::~UnrealEcsSystemScheduler()
{
	Reset();
}

void UnrealEcsSystemScheduler::Reset()
{
	for (auto allocatedTask : AllocatedTasks) { delete allocatedTask; }
	for (auto allocateGraphTask : AllocatedGraphTasks) { delete allocateGraphTask; }
	for (auto allocatedChain : AllocatedChains)	{ delete allocatedChain; }

	AllocatedTasks.Reset();
	AllocatedGraphTasks.Reset();
	AllocatedChains.Reset();
	sysTaskChains.Reset();
	waitingTasks.Reset();
	pendingTasks.Reset();
}
