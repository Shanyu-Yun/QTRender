# RenderCore库配置

# 创建RenderCore接口库（作为聚合库）
add_library(RenderCore INTERFACE)

# 查找并链接Vulkan相关库
find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

# 添加子目录
add_subdirectory(RenderGraph)
add_subdirectory(Resource)
add_subdirectory(Scene)
add_subdirectory(VulkanCore)

# 链接子模块库
target_link_libraries(RenderCore INTERFACE
    RenderGraph
    RenderCore::ResourceManager
    RenderCore::Scene
    RenderCore::VulkanCore
)

# 设置包含目录
target_include_directories(RenderCore INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(RenderCore INTERFACE
    Vulkan::Vulkan
    glm::glm
    GPUOpen::VulkanMemoryAllocator
)

# 安装规则
install(TARGETS RenderCore
    EXPORT RenderCoreTargets
    ARCHIVE DESTINATION lib
)

