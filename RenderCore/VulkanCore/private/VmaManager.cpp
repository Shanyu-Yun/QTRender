#include "VmaManager.hpp"

namespace vkcore
{
VmaAllocator VmaManager::s_Allocator = nullptr;
VmaVulkanFunctions VmaManager::s_VulkanFunctions = {};
bool VmaManager::s_Initialized = false;

void VmaManager::Initialize(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device)
{
    if (s_Initialized)
        return;

    s_VulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    s_VulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.instance = instance;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.pVulkanFunctions = &s_VulkanFunctions;

    VkResult result = vmaCreateAllocator(&allocatorInfo, &s_Allocator);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create VMA Allocator");
    }
    s_Initialized = true;
}

void VmaManager::cleanup()
{
    if (s_Initialized)
    {
        vmaDestroyAllocator(s_Allocator);
        s_Allocator = nullptr;
        s_Initialized = false;
    }
}

VmaAllocator VmaManager::getAllocator()
{
    if (!s_Initialized)
    {
        throw std::runtime_error("VmaManager is not initialized. Call Initialize() first.");
    }
    return s_Allocator;
}

} // namespace vkcore
