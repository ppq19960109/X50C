cmake_minimum_required(VERSION 3.10)
set (APP_NAME linktool)

# set the project name
project(${APP_NAME})
# set the project name and version
project(${APP_NAME} VERSION 1.0)

message(STATUS "PROJECT_SOURCE_DIR:${PROJECT_SOURCE_DIR}")
message(STATUS "PROJECT_BINARY_DIR:${PROJECT_BINARY_DIR}")
message(STATUS "CMAKE_CURRENT_SOURCE_DIR:${CMAKE_CURRENT_SOURCE_DIR}")

set (CMAKE_INSTALL_PREFIX "${PROJECT_BINARY_DIR}/install")
# set (CROSS_COMPILE arm-rockchip-linux-gnueabihf-)

set (CMAKE_C_COMPILER ${CROSS_COMPILE}gcc)
set (CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++)
# set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_STANDARD 11)

# include(CheckCXXCompilerFlag)
# CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
# CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
# if(COMPILER_SUPPORTS_CXX11)
#         message(STATUS "The compiler ${CMAKE_CXX_COMPILER} CXX11")
#         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
# elseif(COMPILER_SUPPORTS_CXX0X)
#         message(STATUS "The compiler ${CMAKE_CXX_COMPILER} CXX0X")
#         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
# else()
#         message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
# endif()
# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
set (SRC_DIRS 
base64        
cJSON
klib
signall
tcp
tcp/epoll
tcp/thread
timer
tool
)
include_directories (${SRC_DIRS})
FOREACH(SRC_DIR ${SRC_DIRS})
        aux_source_directory(${SRC_DIR} SRC_FILES)
ENDFOREACH()

message(STATUS "SRC_DIRS:${SRC_DIRS}")
message(STATUS "SRC_FILES:${SRC_FILES}")

# link_libraries(-ldl -lm pthread -lrt)
# add the executable
add_library(${APP_NAME} STATIC ${SRC_FILES})
# target_include_directories(${APP_NAME} PUBLIC ${SRC_DIRS})
# target_link_libraries(${APP_NAME} -ldl -lm pthread -lrt)

get_property(HEADER_DIRS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
message(STATUS "HEADER_DIRS:${HEADER_DIRS}")

# 指定安装路径
install (TARGETS ${APP_NAME} 
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install (DIRECTORY ${SRC_DIRS} DESTINATION include
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "install*" EXCLUDE
        PATTERN "test" EXCLUDE
        PATTERN "example" EXCLUDE
        PATTERN ".vscode" EXCLUDE
        REGEX "${CMAKE_CURRENT_BINARY_DIR}" EXCLUDE)

        #cmake -DCMAKE_INSTALL_PREFIX=<你想要安装的路径>