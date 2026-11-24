# Examples CMakeLists.txt

add_executable(demo_app
    demo_app.cpp
    command_parser.cpp
)

target_include_directories(demo_app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(demo_app
    PRIVATE
        phone_projection_middleware
)

# Install example
install(TARGETS demo_app
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
)