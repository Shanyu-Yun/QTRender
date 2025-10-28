# RenderCore库配置
cmake_minimum_required(VERSION 3.20)

# 创建RenderCore静态库
add_library(RenderCore STATIC)

# 设置C++标准 [[memory:8770539]]
set_target_properties(RenderCore PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 收集所有源文件
file(GLOB_RECURSE RENDERCORE_HEADERS
    "public/*.hpp"
)

file(GLOB_RECURSE RENDERCORE_SOURCES
    "private/*.cpp"
)

# 添加源文件到库
target_sources(RenderCore PRIVATE
    ${RENDERCORE_SOURCES}
)

# 设置包含目录
target_include_directories(RenderCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/public
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera/public
    ${CMAKE_CURRENT_SOURCE_DIR}/ResourceManager/public
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene/public
    ${CMAKE_CURRENT_SOURCE_DIR}/VulkanCore/public
)

target_include_directories(RenderCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/private
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera/private
    ${CMAKE_CURRENT_SOURCE_DIR}/ResourceManager/private
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene/private
    ${CMAKE_CURRENT_SOURCE_DIR}/VulkanCore/private
)

# 查找并链接Vulkan相关库
find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

# 链接库
target_link_libraries(RenderCore PUBLIC
    Vulkan::Vulkan
    glm::glm
    GPUOpen::VulkanMemoryAllocator
)

# 添加子目录
add_subdirectory(Camera)
add_subdirectory(ResourceManager)
add_subdirectory(Scene)
add_subdirectory(VulkanCore)

# 链接子模块库
target_link_libraries(RenderCore PUBLIC
    RenderCore::Camera
    RenderCore::ResourceManager
    RenderCore::Scene
    RenderCore::VulkanCore
)

# 设置编译定义
target_compile_definitions(RenderCore PRIVATE
    RENDERCORE_EXPORTS
)

# 设置输出目录
set_target_properties(RenderCore PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 安装规则
install(TARGETS RenderCore
    ARCHIVE DESTINATION lib
)

install(DIRECTORY public/
    DESTINATION include/RenderCore
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
