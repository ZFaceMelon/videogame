add_library(engine STATIC
    src/api.cpp
    src/engine.cpp
    src/gl_loader.cpp
    src/renderer.cpp
)

target_include_directories(engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(engine PUBLIC glfw)

if(WIN32)
    target_link_libraries(engine PUBLIC opengl32)
elseif(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
    target_link_libraries(engine PUBLIC
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
    )
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(engine PUBLIC OpenGL::GL)
endif()

# Write OCaml link flags after each engine build (avoids VS multi-config file(GENERATE) errors).
add_custom_command(
    TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DOUT_FILE=${CMAKE_SOURCE_DIR}/game/config/link_flags.sexp
        -DENGINE_LIB=$<TARGET_LINKER_FILE:engine>
        -DGLFW_LIB=$<TARGET_LINKER_FILE:glfw>
        -P ${CMAKE_SOURCE_DIR}/cmake/write_link_flags.cmake
    VERBATIM
)
