cmake_minimum_required(VERSION 3.20)

# 设置项目名称和版本
project(Render VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准 [[memory:8770539]]
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置vcpkg工具链
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

# 设置Qt6路径
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.2/msvc2022_64" ${CMAKE_PREFIX_PATH})

# 设置构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 设置编译选项
if(MSVC)
    add_compile_options(/W4 /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()


# 查找必要的包
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# 启用Qt6的MOC、UIC和RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 添加子目录
add_subdirectory(Render)
add_subdirectory(UI)

# 创建主可执行文件
add_executable(${PROJECT_NAME}
    main.cpp
)

# 链接库
target_link_libraries(${PROJECT_NAME}
    RenderCore
    Renderer
    UI
    Qt6::Core
    Qt6::Widgets
)

# 设置包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 设置目标属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 自动复制Qt6 DLL到输出目录
if(WIN32)
    set(QT_BIN_DIR "C:/Qt/6.9.2/msvc2022_64/bin")
    
    # 根据当前配置（Debug/Release）复制对应的 Qt DLL（使用 generator expressions 选择带 d 后缀的 Debug 版）
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "正在复制 Qt6 DLL..."
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_BIN_DIR}/$<IF:$<CONFIG:Debug>,Qt6Cored.dll,Qt6Core.dll>" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_BIN_DIR}/$<IF:$<CONFIG:Debug>,Qt6Guid.dll,Qt6Gui.dll>" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QT_BIN_DIR}/$<IF:$<CONFIG:Debug>,Qt6Widgetsd.dll,Qt6Widgets.dll>" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "复制Qt6 DLL到输出目录"
    )
endif()

# 安装规则
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)