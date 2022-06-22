cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME echo-server)
project(${PROJECT_NAME} CXX)

set(CMAKE_CXX_STANDARD 20)

##########################################################
# User configurable options of the template
##########################################################
# Note: symbols like WARNINGS_AS_ERRORS in configuration are intentionally variables
# and not CMake options --using options creates too much problem for students.

#! It is a good practice to set "WARNINGS_AS_ERRORS" ON,
#  but sometimes it creates too much trouble, so default is OFF.
set(WARNINGS_AS_ERRORS OFF)

#! Always use PVS Studio while developing. 
set(ENABLE_PVS_STUDIO OFF)

#! Select appropriate sanitizers.
#  Definitely enable sanitizers while developing.
#  Disable it for the production builds and before submitting for grading.
#  Only one of Memory (MSAN), Address (ASAN), or Thread (TSan)
#  sanitizers is applicable at the time -- the first defined.

#! UndefinedBehaviorSanitizer (UBSan)
#  Info: https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
set(ENABLE_UBSan OFF)
#! AddressSanitizer -- detects use after free or after scope exit,
#  memory overflows and leaks.  
#  Info: https://github.com/google/sanitizers/wiki/AddressSanitizer
set(ENABLE_ASAN OFF)
#! ThreadSanitizer -- detects data races.
set(ENABLE_TSan OFF)
#! MemorySanitizer detects uninitialized memory reads 
#  Info: https://github.com/google/sanitizers/wiki/MemorySanitizer
set(ENABLE_MSAN OFF)


#! Be default -- build release version if not specified otherwise.
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# Warnings as errors should be imported here -- do not move this line
include(cmake/CompilerWarnings.cmake)

##########################################################
# Project files, packages, libraries and so on
##########################################################

#! Prepare libraries
# Boost
find_package(Boost 1.71.0 COMPONENTS system REQUIRED)
set(BOOST_KEY_WORD boost_lib)

# Standard Threads
find_package(Threads REQUIRED)
set(THREADS_KEY_WORD thread_lib)

# Google Log
find_package(PkgConfig REQUIRED)
pkg_check_modules(glog REQUIRED libglog)
set(GLOG_KEY_WORD glog_lib)

# Google Flags for glog configurations
find_package(gflags REQUIRED)
set(GFLAGS_KEY_WORD gflags_lib)

#! Common Sources
set(COMMON_SRC
        src/common/io.cpp include/common/io.h include/common/defines.h
        src/common/socket.cpp include/common/socket.h
        src/common/logging.cpp include/common/logging.h
        include/common/thread_safe_queue.h
        include/common/thread_safe_radio_queue.h
        )

#! Specify targets
set(SERVER_TARGETS
        echo_server_simple
        echo_server_simple_threaded
        echo_server_custom_thread_pool
        echo_server_boost_asio
        echo_server_boost_asio_threaded
        )

#! Specify libs to link
set(LINK_LIBS_echo_server_simple
        )
set(LINK_LIBS_echo_server_simple_threaded
        ${THREADS_KEY_WORD}
        )
set(LINK_LIBS_echo_server_custom_thread_pool
        ${THREADS_KEY_WORD}
        )
set(LINK_LIBS_echo_server_boost_asio
        ${BOOST_KEY_WORD}
        )
set(LINK_LIBS_echo_server_boost_asio_threaded
        ${BOOST_KEY_WORD}
        ${THREADS_KEY_WORD}
        )

#! Compile Common Lib
add_library(common STATIC ${COMMON_SRC})
target_include_directories(common PUBLIC include)
# link Google Logs
target_include_directories(common PRIVATE ${glog_INCLUDE_DIRS})
target_link_libraries(common ${glog_LIBRARIES})
# Google Flags
target_include_directories(common PRIVATE ${gflags_INCLUDE_DIRS})
target_link_libraries(common ${gflags_LIBRARIES})

#! Project executables source compilation
foreach (TARGET ${SERVER_TARGETS})
    MESSAGE("Compiling target: ${TARGET}")
    add_executable(${TARGET} main.cpp
            src/${TARGET}.cpp include/${TARGET}.h
            )
    string(TOUPPER ${TARGET} TARGET_SPECIFIER)
    target_compile_definitions(${TARGET} PUBLIC ${TARGET_SPECIFIER})

    # Link common library by default
    target_link_libraries(${TARGET} common)
    target_include_directories(${TARGET} PRIVATE include)

    if ($<IN_LIST:${BOOST_KEY_WORD},LINK_LIBS_${TARGET}>)
        # Boost
        target_include_directories(${TARGET} PRIVATE ${Boost_INCLUDE_DIR})
        target_link_libraries(${TARGET} ${Boost_LIBRARIES})
    elseif ($<IN_LIST:${THREAD_KEY_WORD},LINK_LIBS_${TARGET}>)
        # Thread
        target_link_libraries(${TARGET} ${CMAKE_THREAD_LIBS_INIT})
    endif ()

endforeach ()


##########################################################
# Fixed CMakeLists.txt part 
##########################################################

foreach (TARGET ${SERVER_TARGETS})
    INSTALL(TARGETS ${TARGET}
            DESTINATION bin)
endforeach ()

# Define ALL_TARGETS variable to use in PVS and Sanitizers
set(ALL_TARGETS ${SERVER_TARGETS})

# Include CMake setup
include(cmake/main-config.cmake)
