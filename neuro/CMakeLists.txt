cmake_minimum_required(VERSION 3.20)
project(neuro LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

# Enable ARC for Objective-C++
add_compile_options("$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")

# Build GLFW statically from source so the app is 100% standalone without external dylibs
include(FetchContent)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.9
)
FetchContent_MakeAvailable(glfw)

find_package(OpenGL REQUIRED)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui")

set(SOURCES
    neuro.cpp
    platform_mac.mm
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(neuro MACOSX_BUNDLE ${SOURCES})

target_include_directories(neuro PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/util
    ${OPENGL_INCLUDE_DIR}
)

# Link GLFW statically + macOS system frameworks and system curl
target_link_libraries(neuro PRIVATE
    glfw
    "-lcurl"
    ${OPENGL_LIBRARIES}
    "-framework Cocoa"
    "-framework Carbon"
    "-framework ApplicationServices"
    "-framework IOKit"
    "-framework Security"
    "-framework CoreFoundation"
    "-framework OpenGL"
)

set_target_properties(neuro PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    MACOSX_BUNDLE_BUNDLE_NAME "neuro"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.neuro.macro"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
)
