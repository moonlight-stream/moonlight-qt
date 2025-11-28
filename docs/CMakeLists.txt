cmake_minimum_required(VERSION 3.10)
project(moonlight-qt-docs)

# Documentation
option(BUILD_DOCS "Build documentation" ON)
if(BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html)
        set(DOXYGEN_INPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../app)
        set(DOXYGEN_README_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../README.md)
        
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
                       ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                       @ONLY)
        
        file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIRECTORY})
        
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
        
        message(STATUS "Doxygen documentation will be generated in ${DOXYGEN_OUTPUT_DIRECTORY}")
    else()
        message(WARNING "Doxygen not found. Documentation will not be generated.")
    endif()
endif()

