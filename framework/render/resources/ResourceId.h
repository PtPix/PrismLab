#pragma once
#include <atomic>
#include <cstdint>
namespace prism::gpu
{
    using ResourceId = uint64_t;
    inline ResourceId AllocateResourceId()
    {
        static std::atomic<ResourceId> next{1};
        return next.fetch_add(1, std::memory_order_relaxed);
    }
}
