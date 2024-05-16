#1.cmake verson，指定cmake版本
cmake_minimum_required(VERSION 3.0)

#交叉编译工具链
set(CMAKE_C_COMPILER aarch64-v01c01-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-v01c01-linux-gnu-g++)


add_definitions(-DSZ)

#C++  版本
set(CMAKE_CXX_STANDARD 14)

#输出目录
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build/)


#指定项目的名称
PROJECT(zmodem)

#头文件
INCLUDE_DIRECTORIES(
    ./
    )

#source directory，源文件目录
file(GLOB_RECURSE SRCS
    crctab.c
    lrz.c
    lsz.c
    uart.c
    zm.c
    zreadline.c
    )

#编译选项
add_compile_options(-g  -fPIC )


#设置编译模式
set(CMAKE_BUILD_TYPE "Release" )

#瘦身 strip
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_Cxxf_FLAGS_RELEASE} -s")
set(CMAKE_BUILD_TYPE "RELEASE")



#单单生成库链接到海思平台
#add_library(zmodem ${SRCS})

#库目录
#file(GLOB opencv_lib "./lib/*.a")
#file(GLOB opencv_other_lib "./lib/opencv4/3rdparty/*.a")


#执行程序
ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME}
    pthread
    )




