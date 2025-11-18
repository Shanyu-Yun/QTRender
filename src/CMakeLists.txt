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

# 使用 windeployqt 自动部署 Qt 依赖
if(WIN32)
    # 查找 windeployqt 工具 (通常在 qmake 同级目录)
    if(TARGET Qt6::qmake)
        get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    endif()

    if(WINDEPLOYQT_EXECUTABLE)
        # 添加构建后步骤
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" 
                --verbose 0
                --no-translations
                --no-opengl-sw
                --no-compiler-runtime
                "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMENT "正在运行 windeployqt 部署 Qt 依赖..."
        )
    else()
        message(WARNING "未找到 windeployqt，请确保 Qt 环境配置正确。")
    endif()
endif()

# 安装规则
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)