#pragma once

struct FSystemReadWriteUsage
{
	TSet<int32> Reads;
	TSet<int32> Writes;
};