#include "RenderCore/VulkanCore/public/CommandPoolManager.hpp"
#include "RenderCore/VulkanCore/public/Descriptor.hpp"
#include "RenderCore/VulkanCore/public/Device.hpp"
#include "RenderCore/VulkanCore/public/Pipeline.hpp"
#include "RenderCore/VulkanCore/public/ShaderManager.hpp"
#include "RenderCore/VulkanCore/public/SwapChain.hpp"
#include "RenderCore/VulkanCore/public/VKResource.hpp"
#include "UI/MainWindow.hpp"
#include "UI/VulkanContainer.hpp"
#include "UI/VulkanWindow.hpp"
#include <QApplication>
#include <QTimer>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <vma/vk_mem_alloc.h>

/**
 * @brief 从文件加载 SPIR-V 字节码
 * @param filename SPIR-V 文件路径
 * @return std::vector<uint32_t> SPIR-V 字节码
 */
std::vector<uint32_t> loadSPIRV(const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        throw std::runtime_error("无法打开着色器文件: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()), fileSize);
    file.close();

    return buffer;
}

/**
 * @brief 渲染器类 - 封装完整的渲染循环
 */
class TriangleRenderer
{
  public:
    TriangleRenderer(vkcore::Device &device, vk::SurfaceKHR surface, VulkanWindow *window)
        : m_device(device), m_surface(surface), m_window(window)
    {
        initVulkanResources(surface);
    }

    ~TriangleRenderer()
    {
        cleanup();
    }

    void renderFrame()
    {
        if (!m_initialized)
            return;

        try
        {
            uint32_t currentFrame = m_swapchain->getCurrentFrameIndex();

            // 1. 获取下一个交换链图像（内部会等待 fence 并重置）
            uint32_t imageIndex;
            vk::Result result = m_swapchain->acquireNextImage(imageIndex);

            if (result == vk::Result::eErrorOutOfDateKHR)
            {
                recreateSwapchain();
                return;
            }
            else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
            {
                throw std::runtime_error("获取交换链图像失败");
            }

            // 2. 使用当前帧对应的命令缓冲区
            auto &cmd = m_commandBuffers[currentFrame];

            // 重置命令缓冲区（fence 已在 acquireNextImage 中等待）
            cmd->reset();

            vk::CommandBufferBeginInfo beginInfo{};
            beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
            cmd->begin(beginInfo);
            recordCommandBuffer(*cmd, imageIndex);
            cmd->end();

            // 3. 提交命令缓冲区
            // 等待 per-frame 的 imageAvailable 信号量
            // 发出 per-image 的 renderFinished 信号量
            vk::Semaphore waitSemaphores[] = {m_swapchain->getImageAvailableSemaphore(currentFrame)};
            vk::Semaphore signalSemaphores[] = {m_swapchain->getRenderFinishedSemaphore(imageIndex)};
            vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

            vk::SubmitInfo submitInfo{};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &(*cmd);
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            m_device.getGraphicsQueue().submit(submitInfo, m_swapchain->getInFlightFence(currentFrame));

            // 4. 呈现图像（等待 per-image 的 renderFinished 信号）
            result = m_swapchain->present(m_swapchain->getRenderFinishedSemaphore(imageIndex), imageIndex);

            if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
            {
                recreateSwapchain();
            }
            else if (result != vk::Result::eSuccess)
            {
                throw std::runtime_error("呈现图像失败");
            }

            // 5. 前进到下一帧
            m_swapchain->advanceToNextFrame();

            m_frameCount++;
        }
        catch (const std::exception &e)
        {
            std::cerr << "渲染失败: " << e.what() << std::endl;
        }
    }

    uint64_t getFrameCount() const
    {
        return m_frameCount;
    }

  private:
    void initVulkanResources(vk::SurfaceKHR surface)
    {
        // 1. 创建 VMA 分配器
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.instance = static_cast<VkInstance>(m_device.getInstance());
        allocatorInfo.physicalDevice = static_cast<VkPhysicalDevice>(m_device.getPhysicalDevice());
        allocatorInfo.device = static_cast<VkDevice>(m_device.get());
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

        if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS)
        {
            throw std::runtime_error("创建 VMA 分配器失败");
        }

        // 2. 创建交换链
        m_swapchain = std::make_unique<vkcore::SwapChain>(surface, m_device, m_allocator);
        std::cout << "交换链创建成功:" << std::endl;
        std::cout << "  格式: " << vk::to_string(m_swapchain->getSwapchainFormat()) << std::endl;
        std::cout << "  尺寸: " << m_swapchain->getSwapchainExtent().width << "x"
                  << m_swapchain->getSwapchainExtent().height << std::endl;

        // 3. 创建命令池管理器
        uint32_t graphicsQueueFamilyIndex = m_device.getGraphicsQueueFamilyIndices();
        m_commandPoolManager = std::make_unique<vkcore::CommandPoolManager>(m_device, graphicsQueueFamilyIndex);

        // 分配每帧的命令缓冲区
        for (int i = 0; i < 2; i++)
        {
            m_commandBuffers[i] = m_commandPoolManager->allocate();
        }

        // 4. 创建着色器管理器并加载着色器
        m_shaderManager = std::make_unique<vkcore::ShaderManager>(m_device);
        loadShaders();

        // 5. 创建红色纹理
        createRedTexture();

        // 6. 创建 Descriptor
        createDescriptors();

        // 7. 创建图形管线
        createPipeline();

        m_initialized = true;
        std::cout << "Vulkan 渲染资源初始化完成\n" << std::endl;
    }

    void loadShaders()
    {
        std::cout << "\n=== 加载着色器 ===" << std::endl;

        try
        {
            // 使用纹理着色器
            auto fragCode = loadSPIRV("E:/Github_repo/QTRender/shaders/spv/triangle_texture.frag.spv");
            auto vertCode = loadSPIRV("E:/Github_repo/QTRender/shaders/spv/triangle_texture.vert.spv");

            m_vertShader = m_shaderManager->createShaderModule("triangle_texture.vert", vertCode,
                                                               vk::ShaderStageFlagBits::eVertex);
            m_fragShader = m_shaderManager->createShaderModule("triangle_texture.frag", fragCode,
                                                               vk::ShaderStageFlagBits::eFragment);

            std::cout << "✓ 顶点着色器: triangle_texture.vert.spv" << std::endl;
            std::cout << "✓ 片段着色器: triangle_texture.frag.spv" << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "✗ 着色器加载失败: " << e.what() << std::endl;
            throw;
        }

        std::cout << "===================\n" << std::endl;
    }

    void createRedTexture()
    {
        std::cout << "\n=== 创建红色纹理 ===" << std::endl;

        // 1. 使用 ImageDesc 描述纹理属性
        vkcore::ImageDesc imageDesc = {};
        imageDesc.imageType = vk::ImageType::e2D;
        imageDesc.format = vk::Format::eR8G8B8A8Unorm;
        imageDesc.extent = vk::Extent3D{1, 1, 1};
        imageDesc.mipLevels = 1;
        imageDesc.arrayLayers = 1;
        imageDesc.samples = vk::SampleCountFlagBits::e1;
        imageDesc.tiling = vk::ImageTiling::eOptimal;
        imageDesc.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        imageDesc.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;

        // 2. 使用 Image 类创建纹理（自动管理内存）
        m_texture = std::make_unique<vkcore::Image>("RedTexture", m_device, m_allocator, imageDesc);

        // 3. 创建 staging buffer 并上传红色像素数据
        uint32_t pixelData = 0xFF0000FF; // RGBA: 红色

        vkcore::BufferDesc stagingDesc = {};
        stagingDesc.size = 4; // 4 bytes (RGBA)
        stagingDesc.usageFlags = vk::BufferUsageFlagBits::eTransferSrc;
        stagingDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
        stagingDesc.allocationCreateFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        auto stagingBuffer = std::make_unique<vkcore::Buffer>("StagingBuffer", m_device, m_allocator, stagingDesc);

        // 写入数据到 staging buffer
        stagingBuffer->write(&pixelData, 4, 0);

        // 4. 使用命令缓冲区复制数据到图像
        auto cmd = m_commandPoolManager->allocate();
        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cmd->begin(beginInfo);

        // 转换布局：Undefined -> TransferDst
        vk::ImageMemoryBarrier barrier = {};
        barrier.oldLayout = vk::ImageLayout::eUndefined;
        barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_texture->get();
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
                             vk::DependencyFlags{}, nullptr, nullptr, barrier);

        // 复制 buffer 到 image
        vk::BufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = vk::Offset3D{0, 0, 0};
        region.imageExtent = vk::Extent3D{1, 1, 1};

        cmd->copyBufferToImage(stagingBuffer->get(), m_texture->get(), vk::ImageLayout::eTransferDstOptimal, 1,
                               &region);

        // 转换布局：TransferDst -> ShaderReadOnly
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                             vk::DependencyFlags{}, nullptr, nullptr, barrier);

        cmd->end();

        // 提交命令并等待完成
        vk::SubmitInfo submitInfo = {};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &(*cmd);
        m_device.getGraphicsQueue().submit(submitInfo, nullptr);
        m_device.getGraphicsQueue().waitIdle();

        // staging buffer 会自动销毁（RAII）

        // 5. 创建 Sampler
        vk::SamplerCreateInfo samplerInfo = {};
        samplerInfo.magFilter = vk::Filter::eLinear;
        samplerInfo.minFilter = vk::Filter::eLinear;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = vk::CompareOp::eAlways;
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

        m_sampler = m_device.get().createSampler(samplerInfo);

        std::cout << "✓ 1x1 红色纹理创建成功" << std::endl;
        std::cout << "===================\n" << std::endl;
    }

    void createDescriptors()
    {
        std::cout << "\n=== 创建 Descriptor ===" << std::endl;

        // 1. 创建 Descriptor 分配器和布局缓存
        m_descriptorAllocator = std::make_unique<vkcore::DescriptorAllocator>(m_device);
        m_descriptorLayoutCache = std::make_unique<vkcore::DescriptorLayoutCache>(m_device);

        // 2. 使用 DescriptorLayoutBuilder 创建 Descriptor Set Layout
        auto layout = vkcore::DescriptorLayoutBuilder::begin(m_descriptorLayoutCache.get())
                          .addBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
                          .build();

        // 3. 分配 Descriptor Set
        m_descriptorSet = m_descriptorAllocator->allocate(layout);

        // 4. 使用 DescriptorUpdater 更新 Descriptor Set
        vk::DescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imageInfo.imageView = m_texture->getView(); // 使用 Image 类的 getView()
        imageInfo.sampler = m_sampler;

        vkcore::DescriptorUpdater::begin(m_device, m_descriptorSet)
            .writeImage(0, vk::DescriptorType::eCombinedImageSampler, imageInfo)
            .update();

        std::cout << "✓ Descriptor Set 创建成功" << std::endl;
        std::cout << "===================\n" << std::endl;
    }

    void createPipeline()
    {
        vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        colorBlendAttachment.blendEnable = VK_FALSE;

        // 创建光栅化状态（禁用背面剔除以确保三角形可见）
        vk::PipelineRasterizationStateCreateInfo rasterizationState = {};
        rasterizationState.depthClampEnable = VK_FALSE;
        rasterizationState.rasterizerDiscardEnable = VK_FALSE;
        rasterizationState.polygonMode = vk::PolygonMode::eFill;
        rasterizationState.cullMode = vk::CullModeFlagBits::eNone; // 禁用背面剔除
        rasterizationState.frontFace = vk::FrontFace::eCounterClockwise;
        rasterizationState.depthBiasEnable = VK_FALSE;
        rasterizationState.lineWidth = 1.0f;

        // 重新创建 Descriptor Set Layout 以获取句柄
        auto descriptorSetLayout =
            vkcore::DescriptorLayoutBuilder::begin(m_descriptorLayoutCache.get())
                .addBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
                .build();

        m_pipeline = vkcore::PipelineBuilder(m_device)
                         .addShaderModule(m_vertShader)
                         .addShaderModule(m_fragShader)
                         .setRasterization(rasterizationState)
                         .addColorAttachment(m_swapchain->getSwapchainFormat(), colorBlendAttachment)
                         .addDynamicState(vk::DynamicState::eViewport)
                         .addDynamicState(vk::DynamicState::eScissor)
                         .addDescriptorSetLayout(descriptorSetLayout)
                         .build();

        std::cout << "图形管线创建成功" << std::endl;
    }

    void recordCommandBuffer(vk::CommandBuffer cmd, uint32_t imageIndex)
    {
        // 1. 图像布局转换：Undefined -> ColorAttachment
        vk::ImageMemoryBarrier barrier = {};
        barrier.oldLayout = vk::ImageLayout::eUndefined;
        barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_swapchain->getImage(imageIndex);
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput,
                            vk::DependencyFlags{}, nullptr, nullptr, barrier);

        // 2. 开始动态渲染
        vk::RenderingAttachmentInfo colorAttachment = {};
        colorAttachment.imageView = m_swapchain->getImageView(imageIndex);
        colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.clearValue.color = vk::ClearColorValue(std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f});

        vk::RenderingInfo renderingInfo = {};
        renderingInfo.renderArea.offset = vk::Offset2D{0, 0};
        renderingInfo.renderArea.extent = m_swapchain->getSwapchainExtent();
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

        cmd.beginRendering(renderingInfo);

        // 3. 绑定管线
        m_pipeline->bind(cmd);

        // 4. 绑定 Descriptor Set（纹理）
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->getLayout(), 0, 1, &m_descriptorSet, 0,
                               nullptr);

        // 5. 设置视口和裁剪矩形
        vk::Viewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_swapchain->getSwapchainExtent().width);
        viewport.height = static_cast<float>(m_swapchain->getSwapchainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmd.setViewport(0, 1, &viewport);

        vk::Rect2D scissor = {};
        scissor.offset = vk::Offset2D{0, 0};
        scissor.extent = m_swapchain->getSwapchainExtent();
        cmd.setScissor(0, 1, &scissor);

        // 6. 绘制三角形
        cmd.draw(3, 1, 0, 0);

        cmd.endRendering();

        // 7. 图像布局转换：ColorAttachment -> PresentSrc
        barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eNone;

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe,
                            vk::DependencyFlags{}, nullptr, nullptr, barrier);
    }

    void recreateSwapchain()
    {
        std::cout << "重建交换链..." << std::endl;

        // 等待设备空闲
        m_device.get().waitIdle();

        // 清理旧的交换链相关资源
        m_pipeline.reset();

        // 释放命令缓冲区
        for (auto &cmd : m_commandBuffers)
        {
            cmd.reset();
        }

        m_swapchain->cleanup();

        // 重新创建交换链
        m_swapchain = std::make_unique<vkcore::SwapChain>(m_surface, m_device, m_allocator);

        // 重新分配命令缓冲区
        for (int i = 0; i < 2; i++)
        {
            m_commandBuffers[i] = m_commandPoolManager->allocate();
        }

        // 重新创建管线
        createPipeline();

        std::cout << "交换链重建完成: " << m_swapchain->getSwapchainExtent().width << "x"
                  << m_swapchain->getSwapchainExtent().height << std::endl;
    }

    void cleanup()
    {
        if (!m_initialized)
            return;

        std::cout << "\n清理渲染资源..." << std::endl;

        // 等待设备空闲
        m_device.get().waitIdle();

        // 按照创建的相反顺序清理资源
        m_pipeline.reset();

        // 清理 Descriptor 资源
        m_descriptorAllocator.reset();
        m_descriptorLayoutCache.reset();

        // 清理纹理资源
        if (m_sampler)
        {
            m_device.get().destroySampler(m_sampler);
            m_sampler = nullptr;
        }
        // m_texture 会自动销毁（RAII）
        m_texture.reset();

        m_shaderManager->cleanup();
        m_vertShader.reset();
        m_fragShader.reset();

        // 先释放命令缓冲区
        for (auto &cmd : m_commandBuffers)
        {
            cmd.reset();
        }

        // 再销毁命令池
        m_commandPoolManager.reset();

        // 最后清理交换链
        m_swapchain->cleanup();
        m_swapchain.reset();

        if (m_allocator != VK_NULL_HANDLE)
        {
            vmaDestroyAllocator(m_allocator);
            m_allocator = VK_NULL_HANDLE;
        }

        m_initialized = false;
        std::cout << "渲染资源清理完成（共渲染 " << m_frameCount << " 帧）" << std::endl;
    }

  private:
    vkcore::Device &m_device;
    vk::SurfaceKHR m_surface;
    VulkanWindow *m_window;

    VmaAllocator m_allocator = VK_NULL_HANDLE;
    std::unique_ptr<vkcore::SwapChain> m_swapchain;
    std::unique_ptr<vkcore::CommandPoolManager> m_commandPoolManager;
    std::unique_ptr<vkcore::ShaderManager> m_shaderManager;
    std::shared_ptr<vkcore::ShaderModule> m_vertShader;
    std::shared_ptr<vkcore::ShaderModule> m_fragShader;
    std::unique_ptr<vkcore::Pipeline> m_pipeline;

    // 纹理相关资源
    std::unique_ptr<vkcore::Image> m_texture; ///< 红色纹理（RAII 管理）
    vk::Sampler m_sampler;

    // Descriptor 资源
    std::unique_ptr<vkcore::DescriptorAllocator> m_descriptorAllocator;
    std::unique_ptr<vkcore::DescriptorLayoutCache> m_descriptorLayoutCache;
    vk::DescriptorSet m_descriptorSet;

    std::array<vkcore::CommandBufferHandle, 2> m_commandBuffers; ///< 每帧的命令缓冲区

    bool m_initialized = false;
    uint64_t m_frameCount = 0;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 创建主窗口
    MainWindow mainWindow;

    // 在外部创建VulkanWindow
    VulkanWindow *vulkanWindow = new VulkanWindow();

    // 将VulkanWindow设置到容器中
    mainWindow.getVulkanContainer()->setVulkanWindow(vulkanWindow);

    // 显示主窗口
    mainWindow.show();

    // 创建Vulkan Instance
    QVulkanInstance *vulkanInstance = vulkanWindow->createVulkanInstance();
    if (!vulkanInstance)
    {
        std::cerr << "创建Vulkan Instance失败" << std::endl;
        return -1;
    }

    // 创建Vulkan Surface
    VkSurfaceKHR surface = vulkanWindow->createVulkanSurface(vulkanInstance);
    if (surface == VK_NULL_HANDLE)
    {
        std::cerr << "创建Vulkan Surface失败" << std::endl;
        delete vulkanInstance;
        return -1;
    }

    vk::Instance vkInstance = vulkanInstance->vkInstance();
    vkcore::Device::Config deviceConfig;
    deviceConfig.deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    deviceConfig.vulkan1_3_features = {"dynamicRendering"};
    vkcore::Device device(vkInstance, surface, deviceConfig);
    std::cout << "使用设备: " << device.getPhysicalDevice().getProperties().deviceName << std::endl;

    // 创建渲染器
    std::unique_ptr<TriangleRenderer> renderer;
    try
    {
        renderer = std::make_unique<TriangleRenderer>(device, surface, vulkanWindow);
        std::cout << "渲染器初始化成功\n" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "渲染器初始化失败: " << e.what() << std::endl;
        device.cleanup();
        delete vulkanInstance;
        return -1;
    }

    // 设置渲染循环定时器（60 FPS）
    QTimer *renderTimer = new QTimer(&app);
    QObject::connect(renderTimer, &QTimer::timeout, [&renderer]() {
        if (renderer)
        {
            renderer->renderFrame();
        }
    });

    // 启动渲染循环
    renderTimer->start(16); // ~60 FPS (1000ms / 60 ≈ 16ms)
    std::cout << "渲染循环已启动（目标: 60 FPS）" << std::endl;
    std::cout << "提示: 关闭窗口以退出程序\n" << std::endl;

    // 运行应用程序
    int result = app.exec();

    // 清理资源
    renderTimer->stop();
    delete renderTimer;
    renderer.reset(); // 会调用析构函数清理 Vulkan 资源
    device.cleanup();
    delete vulkanInstance;

    std::cout << "\n程序正常退出" << std::endl;
    return result;
}