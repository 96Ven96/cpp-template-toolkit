cmake_minimum_required(VERSION 3.16)
project(TemplateHelper VERSION 1.0 LANGUAGES CXX)

# ──────────────────────────────────────────────────────────────────────────────
# Core — pure STL, C++20, no Qt dependency
#
# Usage (from a parent project):
#   add_subdirectory(TemplateHelper)
#   target_link_libraries(MyTarget PRIVATE TemplateHelper::Core)
# ──────────────────────────────────────────────────────────────────────────────
add_library(TemplateHelper INTERFACE
    concepts_std.h
    constexpr_helper.h
    enum_flag.h
    enum_keyed_tuple.h
    function_helper.h
    function_traits.h
    template_defines.h
    types_helper.h
)
add_library(TemplateHelper::Core ALIAS TemplateHelper)

target_compile_features(TemplateHelper INTERFACE cxx_std_20)
target_include_directories(TemplateHelper INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# ──────────────────────────────────────────────────────────────────────────────
# Qt variant — layered on top of Core; auto-detects Qt6 (preferred) or Qt5
#
# Usage:
#   target_link_libraries(MyTarget PRIVATE TemplateHelper::Qt)
# ──────────────────────────────────────────────────────────────────────────────
find_package(QT NAMES Qt6 Qt5 QUIET COMPONENTS Core)
if (QT_FOUND)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

    add_library(TemplateHelperQt INTERFACE
        concepts_qt.h
        conversion_types.h
        qt_support_helper.h
    )
    add_library(TemplateHelper::Qt ALIAS TemplateHelperQt)

    target_link_libraries(TemplateHelperQt INTERFACE
        TemplateHelper
        Qt${QT_VERSION_MAJOR}::Core
    )
endif()
