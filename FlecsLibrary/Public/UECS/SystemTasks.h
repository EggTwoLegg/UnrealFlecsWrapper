#pragma once

#include "flecs.h"

DECLARE_STATS_GROUP(TEXT("ECS"), STATGROUP_ECS, STATCAT_Advanced);

namespace flecs
{
	struct world;
}

struct FLECSLIBRARY_API TypeHash
{
	size_t name_hash{0};
	size_t matcher_hash{0};

	bool operator==(const TypeHash& other) const
	{
		return name_hash == other.name_hash;
	}

	template <typename T>
	static constexpr const char* name_detail()
	{
		return __FUNCSIG__;
	}

	static constexpr uint64_t hash_fnv1a(const char* key)
	{
		uint64_t hash = 0xcbf29ce484222325;
		uint64_t prime = 0x100000001b3;

		int i = 0;
		while (key[i])
		{
			uint8_t value = key[i++];
			hash = hash ^ value;
			hash *= prime;
		}

		return hash;
	}

	template <typename T>
	static constexpr size_t hash()
	{
		static_assert(!std::is_reference_v<T>, "dont send references to hash");
		static_assert(!std::is_const_v<T>, "dont send const to hash");
		return hash_fnv1a(name_detail<T>());
	}
};


struct FLECSLIBRARY_API TaskDependencies
{
	template <typename TTask>
	void AddRead()
	{
		readHashes.Add(TypeHash::hash<TTask>());
	}

	template <typename TTask>
	void AddWrite()
	{
		writeHashes.Add(TypeHash::hash<TTask>());
	}

	TArray<size_t, TInlineAllocator<5>> readHashes;
	TArray<size_t, TInlineAllocator<5>> writeHashes;

	// @TODO: Optimize this.
	bool ConflictsWith(const TaskDependencies& otherDeps) const
	{
		for (const auto myWriteHash : writeHashes)
		{
			for (const auto otherReadHash : otherDeps.readHashes)
			{
				if (myWriteHash == otherReadHash) { return true; }
			}

			for (const auto otherWriteHash : otherDeps.writeHashes)
			{
				if (myWriteHash == otherWriteHash) { return true; }
			}
		}

		for (const auto myReadHash : readHashes)
		{
			for (const auto otherWriteHash : otherDeps.writeHashes)
			{
				if (myReadHash == otherWriteHash) { return true; }
			}
		}
		return false;
	}
};


enum class ESysTaskType : uint8_t
{
	GameThread,
	SyncPoint,
	TaskGraph,
	ThreadPool
};


enum class ESysTaskFlags : uint32_t
{
	ExecuteAsync = 1 << 0,
	ExecuteGameThread = 1 << 1,
	HighPriority = 1 << 2,

	NoECS = 1 << 3
};


struct SystemTask
{
	ESysTaskType type;
	ESysTaskFlags flags;
	TaskDependencies deps;
	SystemTask* next = nullptr;

	class SystemTaskChain* ownerGraph{nullptr};
	TFunction<void(flecs::world&)> function;
};


struct GraphTask
{
	SystemTask* original;
	FName TaskName;
	int chainIndex = 0;
	float priorityWeight = 0;
	int predecessorCount = 1;
	TArray<GraphTask*, TInlineAllocator<2>> successors;

	void AddSuccessor(GraphTask* successor)
	{
		successor->predecessorCount++;
		successors.Add(successor);
	}
};


class SystemTaskChain
{
public:
	SystemTaskChain* next{nullptr};
	SystemTask* firstTask{nullptr};
	SystemTask* lastTask{nullptr};
	int sortKey;
	FName name;
	float priority = 1.f;
	TArray<FName, TInlineAllocator<2>> SystemDependencies;

	bool HasSyncPoint() const
	{
		const SystemTask* task = firstTask;
		while (task)
		{
			if (task->type == ESysTaskType::SyncPoint) { return true; }
			task = task->next;
		}
		return false;
	}

	void ExecuteSync(flecs::world& reg) const
	{
		const SystemTask* task = firstTask;
		while (task)
		{
			task->function(reg);
			task = task->next;
		}
	};

	void AddTask(SystemTask* task)
	{
		if (firstTask == nullptr)
		{
			firstTask = task;
			lastTask = task;
		}
		else
		{
			lastTask->next = task;
			lastTask = task;
		}
	}
};


class FLECSLIBRARY_API UnrealEcsSystemScheduler
{
	struct LaunchedTask
	{
		GraphTask* task;
		TaskDependencies dependencies;
		TFuture<void> future;
	};

public:
	UnrealEcsSystemScheduler(flecs::world* inEcsWorld, bool inCacheSystemDependencies);
	
	~UnrealEcsSystemScheduler();

	TArray<SystemTaskChain*> sysTaskChains;
	flecs::world* ecsWorld;
	
	void AddSystemTaskChain(SystemTaskChain* newGraph);
	void CacheSystemDependencies();
	GraphTask* BuildGraphTasks();
	
	void Run(flecs::world& inEcsWorld);
	void Reset();

	void AsyncFinished(GraphTask* task);

	bool LaunchTask(GraphTask* task);

	void AddPending(GraphTask* task, TFuture<void>* future);
	void RemovePending(const GraphTask* task);

	bool CanExecute(const GraphTask* task);

	TArray<GraphTask*> waitingTasks;
	TArray<TSharedPtr<LaunchedTask>> pendingTasks;

	TQueue<GraphTask*> gameThreadTasks;
	GraphTask* syncTask { nullptr };


	FCriticalSection mutex;
	FCriticalSection endmutex;

	TAtomic<int> totalTasks;
	FEvent* threadSignal;


	SystemTask* NewTask();
	GraphTask* NewGraphTask(SystemTask* originalTask);
	SystemTaskChain* NewTaskChain();


	//pooled allocations for easy cleanup
	TArray<SystemTask*> AllocatedTasks;
	TArray<GraphTask*> AllocatedGraphTasks;
	TArray<SystemTaskChain*> AllocatedChains;

	bool cacheSystemDependencies;
	bool didCacheSystemDependencies;
	GraphTask* cachedRootTask { nullptr };
};

class SystemTaskChainBuilder
{
public:
	SystemTaskChainBuilder(FName inName, int inSortKey, class UnrealEcsSystemScheduler* inScheduler, const float priority = 1.f)
	{
		graph = inScheduler->NewTaskChain();
		graph->name = inName;
		graph->sortKey = inSortKey;
		graph->priority = priority;
		scheduler = inScheduler;
	};

	template <typename TTaskFunction>
	void AddTask(const TaskDependencies& deps, TTaskFunction&& taskFunction, ESysTaskType taskType, ESysTaskFlags flags = ESysTaskFlags::ExecuteAsync)
	{
		SystemTask* task = scheduler->NewTask();
		task->deps = deps;
		task->function = std::move(taskFunction);
		task->type = taskType;
		task->flags = flags;
		task->ownerGraph = graph;

		if (!(static_cast<uint32_t>(flags) & static_cast<uint32_t>(ESysTaskFlags::NoECS)))
		{
			task->deps.AddRead<flecs::world>();
		}

		graph->AddTask(task);
	};

	void AddDependency(FName dependency) const
	{
		graph->SystemDependencies.Add(dependency);
	}
	
	template <typename TTaskFunction>
	void AddSyncTask(TTaskFunction&& syncTask)
	{
		SystemTask* task = scheduler->NewTask(); //new SystemTask();		
		task->function = std::move(syncTask);
		task->type = ESysTaskType::TaskGraph; //GameThread;//SyncPoint;
		task->ownerGraph = graph;
		task->deps.AddWrite<flecs::world>();
		task->flags = ESysTaskFlags::ExecuteGameThread;
		graph->AddTask(task);
	};

	SystemTaskChain* FinishGraph() const { return graph; };

	SystemTaskChain* graph;
	UnrealEcsSystemScheduler* scheduler{nullptr};
};
