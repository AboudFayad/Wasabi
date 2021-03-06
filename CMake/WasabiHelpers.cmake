include(ExternalProject)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)
endif()

# Helper function for VS filters
function(assign_source_group)
    foreach(_source IN ITEMS ${ARGN})
        if (IS_ABSOLUTE "${_source}")
            file(RELATIVE_PATH _source_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_source}")
        else()
            set(_source_rel "${_source}")
        endif()
        get_filename_component(_source_path "${_source_rel}" PATH)
        string(REPLACE "/" "\\" _source_path_msvc "${_source_path}")
        source_group("${_source_path_msvc}" FILES "${_source}")
    endforeach()
endfunction(assign_source_group)

function(initialize_submodules)
    find_package(Git QUIET)
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endfunction()

function(enable_all_warnings TARGET_NAME)
    target_compile_options(${TARGET_NAME} PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-Wall -fms-extensions>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)
endfunction()

function(prepare_dependency)
    set(optionArgs "")
    set(oneValueArgs NAME)
    set(multiValueArgs TARGETS)
    cmake_parse_arguments(FUNCTION_ARGS "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    foreach(target IN LISTS FUNCTION_ARGS_TARGETS)
        if (TARGET ${target})
            get_target_property(target_type ${target} TYPE)
            if (target_type STREQUAL "EXECUTABLE" OR target_type STREQUAL "STATIC_LIBRARY" OR target_type STREQUAL "MODULE_LIBRARY" OR target_type STREQUAL "SHARED_LIBRARY")
                if (MSVC)
                    target_compile_options(${target} PRIVATE /W0)
                else()
                    target_compile_options(${target} PRIVATE -w)
                endif()
            endif()
            set_property(TARGET ${target} PROPERTY FOLDER ${FUNCTION_ARGS_NAME})
        endif()
    endforeach()
endfunction()

# include dependency utilities
include(OpenAL)
include(Bullet)
include(GLFW)
include(STB)
include(tinyfiledialogs)
include(GLSL)
include(Assimp)
include(dist)
include(BundleStaticLibraries)
include(LinkToWasabi)
