#include "VKResource.hpp"

namespace vkcore
{

GpuResource::GpuResource(std::string name, vk::Device device) : m_name(name), m_device(device)
{
}

Buffer::Buffer(std::string name, Device &device, VmaAllocator allocator, const BufferDesc &desc)
    : GpuResource(name, device.get()), m_allocator(allocator), m_size(desc.size), m_usage(desc.usageFlags)
{
    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = desc.size;
    bufferInfo.usage = static_cast<VkBufferUsageFlags>(desc.usageFlags);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = desc.memoryUsage;
    allocInfo.flags = desc.allocationCreateFlags;

    //使用临时的VkBuffer句柄接受结果
    VkBuffer rawBuffer = VK_NULL_HANDLE;
    VmaAllocationInfo allocationDetails;

    VkResult result =
        vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &rawBuffer, &m_allocation, &allocationDetails);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create buffer: " + std::to_string(result));
    }

    m_buffer = vk::Buffer(rawBuffer);
    if (allocInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)
    {
        m_mappedData = allocationDetails.pMappedData;
    }
}

Buffer::~Buffer()
{
    release();
}

vk::DeviceAddress Buffer::getDeviceAddress() const
{
    if (!m_buffer)
        throw std::runtime_error("Buffer is not created.");

    vk::BufferDeviceAddressInfo addressInfo = {};
    addressInfo.buffer = m_buffer;
    return m_device.getBufferAddress(addressInfo);
}

void *Buffer::map()
{
    if (!m_buffer)
        throw std::runtime_error("Buffer is not created.");
    if (m_mappedData)
        return m_mappedData; // Already mapped

    if (vmaMapMemory(m_allocator, m_allocation, &m_mappedData) != VK_SUCCESS)
    {
        m_mappedData = nullptr;
    }
    return m_mappedData;
}

void Buffer::ummap()
{
    if (m_mappedData)
    {
        vmaUnmapMemory(m_allocator, m_allocation);
        m_mappedData = nullptr;
    }
}

void Buffer::write(const void *data, vk::DeviceSize size, vk::DeviceSize offset)
{
    if (!m_buffer)
        throw std::runtime_error("Buffer is not created.");
    if (offset + size > m_size)
        throw std::runtime_error("Write range exceeds buffer size.");

    void *mapped = map();
    if (!mapped)
        throw std::runtime_error("Failed to map buffer memory.");

    std::memcpy(static_cast<uint8_t *>(mapped) + offset, data, size);
    ummap();
}

void Buffer::flush(vk::DeviceSize size, vk::DeviceSize offset)
{
    if (!m_buffer)
        throw std::runtime_error("Buffer is not created.");
    if (offset + size > m_size && size != VK_WHOLE_SIZE)
        throw std::runtime_error("Flush range exceeds buffer size.");

    vmaFlushAllocation(m_allocator, m_allocation, offset, size);
}

void Buffer::release()
{
    if (m_buffer)
    {
        vmaDestroyBuffer(m_allocator, static_cast<VkBuffer>(m_buffer), m_allocation);
        m_buffer = nullptr;
        m_allocation = nullptr;
        m_mappedData = nullptr;
        m_size = 0;
    }
}

Image::Image(std::string name, Device &device, VmaAllocator allocator, const ImageDesc &desc)
    : GpuResource(name, device.get()), m_allocator(allocator), m_format(desc.format), m_extent(desc.extent),
      m_mipLevels(desc.mipLevels), m_arrayLayers(desc.arrayLayers), m_usage(desc.usage),
      m_currentLayout(vk::ImageLayout::eUndefined)
{
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = static_cast<VkImageType>(desc.imageType);
    imageInfo.format = static_cast<VkFormat>(desc.format);
    imageInfo.extent = {desc.extent.width, desc.extent.height, desc.extent.depth};
    imageInfo.mipLevels = desc.mipLevels;
    imageInfo.arrayLayers = desc.arrayLayers;
    imageInfo.samples = static_cast<VkSampleCountFlagBits>(desc.samples);
    imageInfo.tiling = static_cast<VkImageTiling>(desc.tiling);
    imageInfo.usage = static_cast<VkImageUsageFlags>(desc.usage);
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = desc.memoryUsage;

    //使用临时的VkImage句柄接受结果
    VkImage rawImage = VK_NULL_HANDLE;
    VmaAllocationInfo allocationDetails;

    VkResult result = vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &rawImage, &m_allocation, &allocationDetails);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image: " + std::to_string(result));
    }

    m_image = vk::Image(rawImage);

    // 创建默认的 ImageView
    vk::ImageViewCreateInfo viewInfo = {};
    viewInfo.image = m_image;
    viewInfo.viewType = (desc.arrayLayers == 1) ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray;
    viewInfo.format = desc.format;

    // 根据格式判断 AspectMask
    vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
    if (desc.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment)
    {
        // 根据格式判断是否包含 Stencil
        if (desc.format == vk::Format::eD32SfloatS8Uint || desc.format == vk::Format::eD24UnormS8Uint ||
            desc.format == vk::Format::eD16UnormS8Uint)
        {
            aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        }
        else
        {
            aspectMask = vk::ImageAspectFlagBits::eDepth;
        }
    }

    viewInfo.subresourceRange.aspectMask = aspectMask;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = desc.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = desc.arrayLayers;

    // 实际创建 ImageView
    m_imageView = m_device.createImageView(viewInfo);
}

Image::~Image()
{
    release();
}

void Image::release()
{
    if (m_imageView)
    {
        m_device.destroyImageView(m_imageView);
        m_imageView = nullptr;
    }
    if (m_image)
    {
        vmaDestroyImage(m_allocator, static_cast<VkImage>(m_image), m_allocation);
        m_image = nullptr;
        m_allocation = nullptr;
        m_format = vk::Format::eUndefined;
        m_extent = vk::Extent3D{1, 1, 1};
        m_mipLevels = 1;
        m_arrayLayers = 1;
        m_usage = vk::ImageUsageFlags();
        m_currentLayout = vk::ImageLayout::eUndefined;
    }
}

} // namespace vkcore