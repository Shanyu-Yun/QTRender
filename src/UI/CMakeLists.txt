# UI模块配置

# 创建UI静态库
add_library(UI STATIC)

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

target_link_libraries(UI PUBLIC
    Qt6::Core
    Qt6::Widgets
)

# 设置编译定义
target_compile_definitions(UI PRIVATE
    UI_EXPORTS
)

# 安装规则
install(TARGETS UI
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/UI
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
