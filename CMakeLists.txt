cmake_minimum_required(VERSION 3.20)

project(mach_override LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 14)

# set(CMAKE_OSX_ARCHITECTURES "arm64 x86_64")
# set(CMAKE_OSX_ARCHITECTURES "x86_64")
# set(CMAKE_OSX_ARCHITECTURES "arm64")
# set(CMAKE_OSX_ARCHITECTURES "i386")
if(${ARCH} STREQUAL "amd64")
    set(ARCH "x86_64")
else()
    message(status "compiling for ${ARCH}")
endif()

set(CMAKE_OSX_ARCHITECTURES "${ARCH}")

message( "ARCH=" ${CMAKE_OSX_ARCHITECTURES} )
set(SPECIFIC_LINK   "${SPECIFIC_LINK};-framework CoreServices")

set(SOURCE "mach_override.cpp")
# file(GLOB_RECURSE SOURCE ./*.cpp ./*.h ./*.hpp)
include("arm64/CMakeLists.txt")
include("ppc/CMakeLists.txt")
include("x86/CMakeLists.txt")
include("libudis86/CMakeLists.txt")

add_library(mach_override_lib SHARED ${SOURCE} )

add_executable(test_mo_${CMAKE_OSX_ARCHITECTURES} ${SOURCE}
    test_mach_override.cpp
)

target_link_libraries(mach_override_lib
        ${SPECIFIC_LINK}
)
target_link_libraries(test_mo_${CMAKE_OSX_ARCHITECTURES}
        ${SPECIFIC_LINK}
)

# message(STATUS "Blork ${TARGET_DIR}")
# ADD_CUSTOM_COMMAND(
#         TARGET test_mo
#         POST_BUILD
#         COMMAND ${CMAKE_COMMAND}
#         ARGS -E copy ${TARGET_DIR}/test_mo ${TARGET_DIR}/test_mo_${CMAKE_OSX_ARCHITECTURES})

# ADD_CUSTOM_COMMAND(
#         TARGET test_mo
#         POST_BUILD
#         COMMAND ${CMAKE_COMMAND}
#         ARGS -E remove ${TARGET_DIR}/test_mo)
