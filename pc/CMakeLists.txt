cmake_minimum_required(VERSION 3.21)
project(ScreenLinkPC VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 6.8 (MSVC 2022 x64) — 通过 CMAKE_PREFIX_PATH 指定安装位置
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(screencap_pc
    src/main.cpp
    src/protocol/FrameCodec.cpp
    src/capture/CaptureEngine.cpp
    src/crypto/CertificateManager.cpp
    src/core/Config.cpp
    src/pairing/PairingManager.cpp
    src/server/TlsServer.cpp
    src/ui/App.cpp
    src/ui/MainWindow.cpp
    src/ui/SettingsDialog.cpp
    resources/app_icon.qrc
)

target_link_libraries(screencap_pc PRIVATE Qt6::Widgets Qt6::Network)

# Windows 子系统：GUI 程序（无控制台窗口，后台运行）
if(WIN32)
    set_target_properties(screencap_pc PROPERTIES WIN32_EXECUTABLE ON)
endif()

# MSVC 工具链补丁：vcvars 的 SDK 探测 bug 导致 LIB 缺 um/shared（kernel32.lib 找不到）
# 这里显式补充 SDK 库搜索路径，保证 CLion / 命令行 / 任何构建方式都能链接
if(MSVC)
    set(SCREENLINK_SDK_LIB "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0")
    if(EXISTS "${SCREENLINK_SDK_LIB}/um/x64")
        link_directories("${SCREENLINK_SDK_LIB}/um/x64")
    endif()
    if(EXISTS "${SCREENLINK_SDK_LIB}/shared/x64")
        link_directories("${SCREENLINK_SDK_LIB}/shared/x64")
    endif()
endif()

# 输出到 bin/
set_target_properties(screencap_pc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# OpenSSL DLL（QSslSocket TLS 后端）：随构建自动复制到输出目录
add_custom_command(TARGET screencap_pc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/bin/libssl-3-x64.dll"
        "${CMAKE_SOURCE_DIR}/resources/bin/libcrypto-3-x64.dll"
        $<TARGET_FILE_DIR:screencap_pc>
)

# 核心模块单元测试（控制台程序）
add_executable(screencap_test
    tests/test_core.cpp
    src/protocol/FrameCodec.cpp
    src/capture/CaptureEngine.cpp
    src/crypto/CertificateManager.cpp
    src/pairing/PairingManager.cpp
)
target_link_libraries(screencap_test PRIVATE Qt6::Widgets Qt6::Network)

# 端到端协议测试（模拟手机端完整流程）
add_executable(screencap_e2e
    tests/test_server.cpp
    src/protocol/FrameCodec.cpp
    src/capture/CaptureEngine.cpp
    src/crypto/CertificateManager.cpp
    src/core/Config.cpp
    src/pairing/PairingManager.cpp
    src/server/TlsServer.cpp
)
target_link_libraries(screencap_e2e PRIVATE Qt6::Widgets Qt6::Network)

# 控制台版服务端宿主（联调/调试）
add_executable(screencap_host
    tests/e2e_host.cpp
    src/protocol/FrameCodec.cpp
    src/capture/CaptureEngine.cpp
    src/crypto/CertificateManager.cpp
    src/core/Config.cpp
    src/pairing/PairingManager.cpp
    src/server/TlsServer.cpp
)
target_link_libraries(screencap_host PRIVATE Qt6::Widgets Qt6::Network)
