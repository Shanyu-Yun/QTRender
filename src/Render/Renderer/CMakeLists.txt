# Renderer库配置

# 创建Renderer静态库
add_library(Renderer STATIC)

# 收集所有源文件
file(GLOB_RECURSE RENDERER_HEADERS
    "public/*.hpp"
)

file(GLOB_RECURSE RENDERER_SOURCES
    "private/*.cpp"
)

# 添加源文件到库（如果有的话）
if(RENDERER_SOURCES)
    target_sources(Renderer PRIVATE
        ${RENDERER_SOURCES}
    )
endif()

# 设置包含目录
target_include_directories(Renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_include_directories(Renderer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/private
)

# 链接 RenderCore
target_link_libraries(Renderer PUBLIC
    RenderCore
)

# 设置编译定义
target_compile_definitions(Renderer PRIVATE
    RENDERER_EXPORTS
)

# 安装规则
install(TARGETS Renderer
    ARCHIVE DESTINATION lib
)

install(DIRECTORY public/
    DESTINATION include/Renderer
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
