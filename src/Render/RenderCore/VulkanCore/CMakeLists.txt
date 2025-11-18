# VulkanCore模块配置

# 创建VulkanCore静态库（实际目标名不使用 ::，后面创建一个 ALIAS）
add_library(VulkanCore STATIC)

# 禁用Qt自动处理，因为VulkanCore模块不使用Qt
set_target_properties(VulkanCore PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

# 收集源文件
file(GLOB_RECURSE VULKANCORE_HEADERS
    "public/*.hpp"
)

file(GLOB_RECURSE VULKANCORE_SOURCES
    "private/*.cpp"
)

# 添加源文件
target_sources(VulkanCore PRIVATE
    ${VULKANCORE_SOURCES}
)

# 设置包含目录
target_include_directories(VulkanCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_include_directories(VulkanCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/private
)

# 链接Vulkan库
find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)


# 创建命名空间别名以便其他 CMakeLists 可以使用 RenderCore::VulkanCore
add_library(RenderCore::VulkanCore ALIAS VulkanCore)


target_link_libraries(VulkanCore PUBLIC
    Vulkan::Vulkan
    glm::glm
    GPUOpen::VulkanMemoryAllocator
)

