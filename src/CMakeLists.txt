# Phone Projection Middleware - Source CMakeLists.txt

# Common library
add_library(pp_common STATIC
    common/common.cpp
)

target_include_directories(pp_common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common
)

target_link_libraries(pp_common
    PUBLIC
        fmt::fmt
        nlohmann_json::nlohmann_json
)

set_target_properties(pp_common PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Platform abstraction library
add_library(pp_platform STATIC
    platform/platform_adapter.cpp
    platform/connection_manager.cpp
    platform/resource_manager.cpp
    platform/input_handler.cpp
    platform/ui_renderer.cpp
    platform/capability_provider.cpp
)

target_include_directories(pp_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform
)

target_link_libraries(pp_platform
    PUBLIC
        pp_common
)

set_target_properties(pp_platform PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Core middleware library
add_library(pp_core STATIC
    core/session_manager.cpp
    core/lifecycle_manager.cpp
    core/event_router.cpp
    core/config_manager.cpp
    core/error_handler.cpp
    core/projection_middleware.cpp
)

target_include_directories(pp_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/core
)

target_link_libraries(pp_core
    PUBLIC
        pp_common
        pp_platform
)

set_target_properties(pp_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Android Auto adapter library
add_library(pp_android_auto STATIC
    android_auto/android_auto_adapter.cpp
)

target_include_directories(pp_android_auto
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/android_auto
)

target_link_libraries(pp_android_auto
    PUBLIC
        pp_common
        pp_platform
        pp_core
)

set_target_properties(pp_android_auto PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Convenience target for all libraries
add_library(phone_projection_middleware INTERFACE)
target_link_libraries(phone_projection_middleware INTERFACE
    pp_common
    pp_platform
    pp_core
    pp_android_auto
)

# Install targets
install(TARGETS pp_common pp_platform pp_core pp_android_auto
    EXPORT PhoneProjectionMiddlewareTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)