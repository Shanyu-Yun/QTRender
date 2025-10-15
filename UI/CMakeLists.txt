# UI模块配置
cmake_minimum_required(VERSION 3.20)

# 创建UI静态库
add_library(UI STATIC)

# 设置C++标准 [[memory:8770539]]
set_target_properties(UI PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 收集源文件
file(GLOB_RECURSE UI_HEADERS
    "*.hpp"
)

file(GLOB_RECURSE UI_SOURCES
    "*.cpp"
)

# 添加源文件
target_sources(UI PRIVATE
    ${UI_SOURCES}
)

# 设置包含目录
target_include_directories(UI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 查找并链接Qt6库
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# 启用Qt6的MOC、UIC和RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

target_link_libraries(UI PUBLIC
    Qt6::Core
    Qt6::Widgets
)

# 设置编译定义
target_compile_definitions(UI PRIVATE
    UI_EXPORTS
)

# 设置输出目录
set_target_properties(UI PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 安装规则
install(TARGETS UI
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/UI
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
