if(NOT OUT_FILE OR NOT ENGINE_LIB OR NOT GLFW_LIB)
    message(FATAL_ERROR "write_link_flags.cmake: missing OUT_FILE, ENGINE_LIB, or GLFW_LIB")
endif()

if(MSVC)
    set(CONTENT "( -cclib \"${ENGINE_LIB}\" -cclib \"${GLFW_LIB}\" -cclib opengl32 )")
else()
    set(CONTENT "( -cclib \"${ENGINE_LIB}\" -cclib \"${GLFW_LIB}\" )")
endif()

file(WRITE "${OUT_FILE}" "${CONTENT}\n")
