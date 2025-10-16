#pragma once
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace vkcore
{
class VmaManager
{
  public:
    static void Initialize(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device);
    static void Cleanup();

    static VmaAllocator GetAllocator();

  private:
    static VmaAllocator s_Allocator;
    static VmaVulkanFunctions s_VulkanFunctions;
    static bool s_Initialized;
};
} // namespace vkcore