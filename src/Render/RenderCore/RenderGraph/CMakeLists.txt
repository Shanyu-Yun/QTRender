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

message(STATUS "RenderGraph module configured")
message(STATUS "  Public headers: ${RENDERGRAPH_PUBLIC_HEADERS}")
message(STATUS "  Sources: ${RENDERGRAPH_SOURCES}")

# 创建RenderGraph库
add_library(RenderGraph STATIC
    ${RENDERGRAPH_SOURCES}
)

# 设置目标属性
target_include_directories(RenderGraph
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 访问其他RenderCore模块
)

# 链接依赖库
target_link_libraries(RenderGraph
    PUBLIC
        VulkanCore
        GPUOpen::VulkanMemoryAllocator
)

# 设置编译特性
target_compile_features(RenderGraph PUBLIC cxx_std_17)

# 添加编译定义
target_compile_definitions(RenderGraph PRIVATE
    $<$<CONFIG:Debug>:RENDERGRAPH_DEBUG>
)

# 设置目标文件夹（用于IDE组织）
set_target_properties(RenderGraph PROPERTIES FOLDER "RenderCore")

# 设置调试信息
if(MSVC)
    target_compile_options(RenderGraph PRIVATE /W4)
else()
    target_compile_options(RenderGraph PRIVATE -Wall -Wextra)
endif()

# 输出信息
message(STATUS "RenderGraph module configured")
message(STATUS "  Public headers: ${RENDERGRAPH_PUBLIC_HEADERS}")
message(STATUS "  Sources: ${RENDERGRAPH_SOURCES}")
