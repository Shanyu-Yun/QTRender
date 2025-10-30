# Camera模块配置
cmake_minimum_required(VERSION 3.20)

# 创建Camera静态库（实际目标名不使用 ::，后面创建一个 ALIAS）
add_library(Camera STATIC)

# 禁用Qt自动处理，因为Camera模块不使用Qt
set_target_properties(Camera PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

# 设置C++标准 [[memory:8770539]]
set_target_properties(Camera PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 收集源文件
file(GLOB_RECURSE CAMERA_HEADERS
    "public/*.hpp"
)

file(GLOB_RECURSE CAMERA_SOURCES
    "private/*.cpp"
)

# 添加源文件
target_sources(Camera PRIVATE
    ${CAMERA_SOURCES}
)

# 设置包含目录
target_include_directories(Camera PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_include_directories(Camera PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/private
)

# 链接glm库用于数学计算
find_package(glm REQUIRED)
target_link_libraries(Camera PUBLIC
    glm::glm
)

# 设置编译定义
target_compile_definitions(Camera PRIVATE
    CAMERA_EXPORTS
)

# 创建命名空间别名以便其他 CMakeLists 可以使用 RenderCore::Camera
add_library(RenderCore::Camera ALIAS Camera)
