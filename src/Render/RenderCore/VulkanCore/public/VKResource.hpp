#pragma once

#include "Device.hpp"
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

/**
 * @file VkResource.hpp
 * @brief Vulkan 资源管理类的定义，包括 Buffer 和 Image 的 RAII 封装
 * @details 使用 VMA (Vulkan Memory Allocator) 进行内存管理，
 *          提供自动的资源生命周期管理和内存分配/释放
 */

namespace vkcore
{
/**
 * @class GpuResource
 * @brief GPU 资源基类，提供通用的资源管理接口
 */
class GpuResource
{
  public:
    GpuResource(std::string name, vk::Device device);

    /**
     * @brief 虚析构函数，支持多态删除
     */
    virtual ~GpuResource() = default;

    /**
     * @brief 获取资源的调试名称
     * @return const std::string& 资源名称的常引用
     */
    inline const std::string &getName() const
    {
        return m_name;
    }

    /**
     * @brief 设置资源的调试名称
     * @param name 新的资源名称
     * @details 设置后可用于 RenderDoc、Nsight 等调试工具中识别资源
     */
    void setName(const std::string &name)
    {
        m_name = name;
    }

  protected:
    std::string m_name;  ///< 资源调试名称
    vk::Device m_device; ///< Device 句柄
};

/**
 * @struct BufferDesc
 * @brief Buffer 创建描述符，用于指定 Buffer 的属性和内存分配策略
 */
struct BufferDesc
{
    vk::DeviceSize size = 0;                                  ///< 缓冲区大小（字节）
    vk::BufferUsageFlags usageFlags = vk::BufferUsageFlags(); ///< 缓冲区用途（如 TransferSrc、VertexBuffer 等）
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO; ///< 内存使用类型（AUTO 会自动选择最优位置）
    VmaAllocationCreateFlags allocationCreateFlags = 0; ///< 分配标志（如 HOST_ACCESS_SEQUENTIAL_WRITE）
};

/**
 * @struct ImageDesc
 * @brief Image 创建描述符，用于指定 Image 的属性和内存分配策略
 */
struct ImageDesc
{
    vk::ImageType imageType = vk::ImageType::e2D;                  ///< 图像类型（1D/2D/3D）
    vk::Format format = vk::Format::eUndefined;                    ///< 图像格式（如 R8G8B8A8_SRGB）
    vk::Extent3D extent = {1, 1, 1};                               ///< 图像尺寸（宽x高x深度）
    uint32_t mipLevels = 1;                                        ///< MIP 级别数量（1 表示无 mipmaps）
    uint32_t arrayLayers = 1;                                      ///< 数组层数量（用于纹理数组）
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1; ///< 采样数（MSAA）
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;            ///< 图像布局（Optimal 表示 GPU 优化）
    vk::ImageUsageFlags usage = vk::ImageUsageFlags();  ///< 图像用途（如 ColorAttachment、Sampled 等）
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO; ///< 内存使用类型（AUTO 会自动选择最优位置）
};

/**
 * @class Buffer
 * @brief Vulkan Buffer 的 RAII 封装，提供自动内存管理
 * @warning 不支持拷贝和移动，使用 unique_ptr/shared_ptr 管理所有权
 */
class Buffer : public GpuResource
{
  public:
    /**
     * @brief 构造函数，创建 Buffer 并分配内存
     * @param device Vulkan 逻辑设备
     * @param allocator VMA 分配器
     * @param desc Buffer 描述符
     * @throws std::runtime_error 如果创建失败
     */
    Buffer(std::string name, Device &device, VmaAllocator allocator, const BufferDesc &desc);

    /**
     * @brief 析构函数，自动释放 Buffer 和内存
     */
    ~Buffer();

    /** 禁用拷贝和移动 */
    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;
    Buffer(Buffer &&) = delete;
    Buffer &operator=(Buffer &&) = delete;

    /**
     * @brief 获取底层 Vulkan Buffer 句柄
     * @return vk::Buffer Vulkan Buffer 句柄
     */
    vk::Buffer get() const
    {
        return m_buffer;
    }

    /**
     * @brief 获取 Buffer 大小
     * @return vk::DeviceSize Buffer 的字节大小
     */
    vk::DeviceSize getSize() const
    {
        return m_size;
    }

    /**
     * @brief 获取 Buffer 使用标志
     * @return vk::BufferUsageFlags Buffer 的使用标志
     */
    vk::BufferUsageFlags getUsage() const
    {
        return m_usage;
    }

    /**
     * @brief 获取 Buffer 的设备地址
     * @return vk::DeviceAddress GPU 端可访问的设备地址
     * @details 用于 GPU 端的缓冲区访问（如光线追踪、间接绘制等）
     * @note 需要 Buffer 创建时包含 eShaderDeviceAddress 用途标志
     */
    vk::DeviceAddress getDeviceAddress() const;

    /**
     * @brief 映射 Buffer 内存到 CPU 地址空间
     * @return void* 映射后的 CPU 可访问指针
     * @throws std::runtime_error 如果映射失败或 Buffer 不支持映射
     * @note 必须调用 unmap() 解除映射
     */
    void *map();

    /**
     * @brief 解除 Buffer 内存映射
     * @note 必须在 map() 之后调用
     */
    void ummap();

    /**
     * @brief 写入数据到 Buffer
     * @param data 源数据指针
     * @param size 写入的字节数
     * @param offset Buffer 内的偏移量（字节）
     * @throws std::runtime_error 如果写入失败
     * @details 内部会自动 map/unmap，适用于一次性写入操作
     */
    void write(const void *data, vk::DeviceSize size, vk::DeviceSize offset = 0);

    /**
     * @brief 刷新内存缓存（确保 CPU 写入对 GPU 可见）
     * @param size 刷新的字节数（VK_WHOLE_SIZE 表示全部）
     * @param offset 刷新的起始偏移量（字节）
     * @details 用于非相干内存（non-coherent memory）确保数据同步
     */
    void flush(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);

  private:
    VmaAllocator m_allocator = nullptr;   ///< VMA 分配器
    VmaAllocation m_allocation = nullptr; ///< VMA 分配句柄

    vk::Buffer m_buffer = nullptr;                         ///< Vulkan Buffer 句柄
    vk::DeviceSize m_size = 0;                             ///< Buffer 大小（字节）
    vk::BufferUsageFlags m_usage = vk::BufferUsageFlags(); ///< Buffer 使用标志
    void *m_mappedData = nullptr;                          ///< 已映射的 CPU 指针（nullptr 表示未映射）

  private:
    /**
     * @brief 释放 Buffer 和内存资源
     * @details 由析构函数调用
     */
    void release();
};

/**
 * @class Image
 * @brief Vulkan Image 的 RAII 封装，提供自动内存和 ImageView 管理
 * @warning 不支持拷贝和移动，使用 unique_ptr/shared_ptr 管理所有权
 */
class Image : public GpuResource
{
  public:
    /**
     * @brief 构造函数，创建 Image、分配内存并创建 ImageView
     * @param device Vulkan 逻辑设备
     * @param allocator VMA 分配器
     * @param desc Image 描述符
     * @throws std::runtime_error 如果创建失败
     */
    Image(std::string name, Device &device, VmaAllocator allocator, const ImageDesc &desc);

    /**
     * @brief 析构函数，自动释放 Image、ImageView 和内存
     */
    ~Image();

    /** 禁用拷贝和移动 */
    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;
    Image(Image &&) = delete;
    Image &operator=(Image &&) = delete;

    /**
     * @brief 获取底层 Vulkan Image 句柄
     * @return vk::Image Vulkan Image 句柄
     */
    inline vk::Image get() const
    {
        return m_image;
    }

    /**
     * @brief 获取 ImageView 句柄
     * @return vk::ImageView 默认创建的 ImageView 句柄
     * @details ImageView 在构造时自动创建，包含所有 mip 级别和数组层
     */
    inline vk::ImageView getView() const
    {
        return m_imageView;
    }

    /**
     * @brief 获取 Image 格式
     * @return vk::Format Image 的像素格式
     */
    inline vk::Format getFormat() const
    {
        return m_format;
    }

    /**
     * @brief 获取 Image 尺寸
     * @return vk::Extent3D Image 的三维尺寸（宽x高x深度）
     */
    inline vk::Extent3D getExtent() const
    {
        return m_extent;
    }

    /**
     * @brief 获取 MIP 级别数量
     * @return uint32_t MIP 级别数量
     */
    inline uint32_t getMipLevels() const
    {
        return m_mipLevels;
    }

    /**
     * @brief 获取数组层数量
     * @return uint32_t 数组层数量
     */
    inline uint32_t getArrayLayers() const
    {
        return m_arrayLayers;
    }

    /**
     * @brief 获取 Image 使用标志
     * @return vk::ImageUsageFlags Image 的使用标志
     */
    inline vk::ImageUsageFlags getUsage() const
    {
        return m_usage;
    }

    /**
     * @brief 获取当前 ImageLayout
     * @return vk::ImageLayout 当前的图像布局
     * @details 用于确定是否需要进行布局转换
     */
    inline vk::ImageLayout getCurrentLayout() const
    {
        return m_currentLayout;
    }

    /**
     * @brief 设置当前 ImageLayout（手动更新跟踪状态）
     * @param layout 新的图像布局
     * @warning 此函数只更新内部跟踪状态，不会执行实际的布局转换！
     *          实际的布局转换需要通过 pipeline barrier 在 CommandBuffer 中执行
     */
    void setCurrentLayout(vk::ImageLayout layout)
    {
        m_currentLayout = layout;
    }

  private:
    VmaAllocator m_allocator = nullptr;   ///< VMA 分配器
    VmaAllocation m_allocation = nullptr; ///< VMA 分配句柄

    vk::Image m_image = nullptr;         ///< Vulkan Image 句柄
    vk::ImageView m_imageView = nullptr; ///< 默认 ImageView 句柄

    vk::Format m_format = vk::Format::eUndefined;                  ///< 图像格式
    vk::Extent3D m_extent = {1, 1, 1};                             ///< 图像尺寸
    uint32_t m_mipLevels = 1;                                      ///< MIP 级别数量
    uint32_t m_arrayLayers = 1;                                    ///< 数组层数量
    vk::ImageUsageFlags m_usage = vk::ImageUsageFlags();           ///< Image 使用标志
    vk::ImageLayout m_currentLayout = vk::ImageLayout::eUndefined; ///< 当前图像布局（手动跟踪）

  private:
    /**
     * @brief 释放 Image、ImageView 和内存资源
     * @details 由析构函数调用
     */
    void release();
};
} // namespace vkcore
