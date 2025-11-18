# RenderGraph模块CMakeLists.txt

# 收集头文件和源文件
file(GLOB RENDERGRAPH_PUBLIC_HEADERS
    "public/*.hpp"
)

file(GLOB RENDERGRAPH_PRIVATE_HEADERS
    "private/*.hpp"
)

file(GLOB RENDERGRAPH_SOURCES
    "private/*.cpp"
)

# 创建RenderGraph库
add_library(RenderGraph STATIC
    ${RENDERGRAPH_SOURCES}
)

# 设置目标属性
target_include_directories(RenderGraph
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 访问其他RenderCore模块
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private
)

# 链接依赖库
target_link_libraries(RenderGraph
    PUBLIC
        VulkanCore
        GPUOpen::VulkanMemoryAllocator
)

# 添加编译定义
target_compile_definitions(RenderGraph PRIVATE
    $<$<CONFIG:Debug>:RENDERGRAPH_DEBUG>
)

# 设置目标文件夹（用于IDE组织）
set_target_properties(RenderGraph PROPERTIES FOLDER "RenderCore")
