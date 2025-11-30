cmake_minimum_required(VERSION 3.19)
project(signaling-server LANGUAGES CXX)

# 设置 Qt 安装路径
# 如果使用VS Code 或其他IDE 自动构建的时候需要修改此路径
set(CMAKE_PREFIX_PATH "f:/Qt/6.8.3/msvc2022_64") 

# 启用自动化功能和 C++ 标准
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt6 所需模块
find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets Widgets)

# 设置编译选项
if (MSVC)
    add_compile_options(
        /utf-8            # 设置 UTF-8 编码
        /Zc:__cplusplus   # 启用 __cplusplus 宏的标准行为
        /permissive-      # 启用更严格的标准兼容性
        /std:c++17        # 使用 C++17 标准
    )
endif()

# 定义源文件和头文件
set(SRCS
    src/main.cpp
    src/Widget.cpp
    src/SignalingServer.cpp
    src/Worker.cpp
)

set(HEADERS
    src/Widget.h
    src/SignalingServer.h
    src/Worker.h
    src/BlockingQueue.hpp
    src/Common.hpp
    src/Test.hpp
)

# 定义 UI 文件
set(UIS
    src/Widget.ui
)
qt_wrap_ui(UI_SOURCES ${UIS})

# 定义资源文件
set(RESOURCES
    src/Widget.qrc
)
qt_add_resources(RESOURCE_SOURCES ${RESOURCES})

# 添加可执行文件
add_executable(${PROJECT_NAME} ${SRCS} ${HEADERS} ${UI_SOURCES} ${RESOURCE_SOURCES})

# 链接 Qt6 模块
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
    Qt6::Widgets
)

# 设置头文件包含路径
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 添加自定义命令，在构建后运行 windeployqt
if (WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_PREFIX_PATH}/bin/windeployqt.exe $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Running windeployqt to deploy Qt dependencies..."
    )
endif()