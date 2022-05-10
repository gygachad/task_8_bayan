cmake_minimum_required(VERSION 3.2)

set(PATCH_VERSION "1" CACHE INTERNAL "Patch version")
set(PROJECT_VESRION 0.0.${PATCH_VERSION})

project(bayan VERSION ${PROJECT_VESRION})

option(WITH_GOOGLE_TEST "Google test" OFF)

configure_file(version.h.in version.h)

file(COPY ${CMAKE_BINARY_DIR}/version.h
        DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Boost COMPONENTS program_options REQUIRED)

include_directories(${Boost_INCLUDE_DIR})

add_executable(bayan main.cpp walker_hash.cpp johnny_walker.cpp version.cpp)

set_target_properties(bayan PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(bayan
    PRIVATE "${CMAKE_BINARY_DIR}"
)

include_directories(
        ${Boost_INCLUDE_DIR}
    )

target_link_libraries(bayan
    ${Boost_LIBRARIES}
    )

if(WITH_GOOGLE_TEST)

    find_package(GTest REQUIRED)
    add_executable(gtest_bayan gtest_bayan.cpp version.cpp)

    set_target_properties(gtest_bayan PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    include_directories(
        ${GTEST_INCLUDE_DIR}
        ${Boost_INCLUDE_DIR}
    )

    target_link_libraries(gtest_bayan
        ${GTEST_LIBRARY}
        ${GTEST_MAIN_LIBRARY}
        ${Boost_LIBRARIES}
    )

    if(UNIX)
        find_package(Threads REQUIRED)
        target_link_libraries(gtest_bayan
            ${CMAKE_THREAD_LIBS_INIT}
        )        
    endif()

endif()

if (MSVC)
    target_compile_options(bayan PRIVATE
        /W4
    )
    if(WITH_GOOGLE_TEST)
        target_compile_options(bayan PRIVATE
            /MT
        )
        target_compile_options(gtest_bayan PRIVATE
            /MT
        )
    endif()
else ()
    target_compile_options(bayan PRIVATE
        -Wall -Wextra -pedantic -Werror
    )
endif()

install(TARGETS bayan RUNTIME DESTINATION bin)

set(CPACK_GENERATOR DEB)

if (WIN32)
    set(CPACK_GENERATOR ZIP)
endif()

set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT example@example.com)
include(CPack)

if(WITH_GOOGLE_TEST)
    enable_testing()
    add_test(gtest_bayan gtest_bayan)
endif()