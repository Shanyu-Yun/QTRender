# ResourceManager模块配置
cmake_minimum_required(VERSION 3.20)

# 创建ResourceManager静态库
add_library(ResourceManager STATIC)

# 禁用Qt自动处理，因为ResourceManager模块不使用Qt
set_target_properties(ResourceManager PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

# 设置C++标准 [[memory:8770539]]
set_target_properties(ResourceManager PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 收集源文件
file(GLOB_RECURSE RESOURCEMANAGER_HEADERS
    "public/*.hpp"
)

file(GLOB_RECURSE RESOURCEMANAGER_SOURCES
    "private/*.cpp"
)

# 添加源文件
target_sources(ResourceManager PRIVATE
    ${RESOURCEMANAGER_SOURCES}
)

# 设置包含目录
target_include_directories(ResourceManager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_include_directories(ResourceManager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/private
)

# 链接Vulkan库
# 链接Vulkan库
find_package(Vulkan REQUIRED)
target_link_libraries(ResourceManager PUBLIC
    Vulkan::Vulkan
)

# 设置编译定义
target_compile_definitions(ResourceManager PRIVATE
    RESOURCEMANAGER_EXPORTS
)

# 创建命名空间别名以便其他 CMakeLists 可以使用 RenderCore::ResourceManager
add_library(RenderCore::ResourceManager ALIAS ResourceManager)
