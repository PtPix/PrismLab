#pragma once
#include <atomic>
#include <cstdint>
namespace Prism::Gpu
{
	using FResourceId = uint64_t;
	inline FResourceId AllocateResourceId()
	{
		static std::atomic<FResourceId> Next{1};
		return Next.fetch_add(1, std::memory_order_relaxed);
	}
} // namespace Prism::Gpu
