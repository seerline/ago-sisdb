#定义工程名,必须在第一行
PROJECT(siscs)

#检查版本 必须大于等于2.6版本
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

#设置显示编译参数 方便验证编译参数
SET(CMAKE_VERBOSE_MAKEFILE 1)

########################运行平台判定和初始化######################

MESSAGE(STATUS "operation system is ${PROJECT} ${CMAKE_SYSTEM}")
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    SET(SHOBJ_CFLAGS "-Wall -fPIC -g -ggdb -std=gnu99 -O2")
    MESSAGE(STATUS "current platform: Linux ")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    MESSAGE(STATUS "current platform: Windows")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "APPLE")
    MESSAGE(STATUS "current platform: APPLE")
    SET(SHOBJ_CFLAGS -Wall -dynamic -fno-common -g -ggdb -std=gnu99 -O2)
ELSE ()
    MESSAGE(STATUS "other platform: ${CMAKE_SYSTEM_NAME}, error!")
ENDIF ()

#检查是否为64位操作系统
IF (CMAKE_SIZEOF_VOID_P MATCHES 4)
  MESSAGE(STATUS "operation system is 32, error!")
ENDIF ()

#########################目录设置#############################
SET(SISCS_PATH ../)  

# 设置头文件目录
INCLUDE_DIRECTORIES(
    .
    ./libuv/include
    ${SISCS_PATH}core
    ${SISCS_PATH}os/linux
	${SISCS_PATH}relying/client)

#设置源文件目录
AUX_SOURCE_DIRECTORY(. DIR_SRCS)
AUX_SOURCE_DIRECTORY(${SISCS_PATH}core DIR_CORE)
AUX_SOURCE_DIRECTORY(${SISCS_PATH}os/linux DIR_SYSTEM)
AUX_SOURCE_DIRECTORY(${SISCS_PATH}relying/client DIR_PLATFORM)

# 把源文件目录设置到环境变量
SET(SRC_LIST ${DIR_SRCS}
	         ${DIR_CORE}
             ${DIR_SYSTEM}
             ${DIR_PLATFORM})

# 设置输出文件目录
SET(LIBRARY_OUTPUT_PATH ../../../bin/)  
########################设置编译类型和参数#####################

#输出信息
MESSAGE(STATUS "FLAGS : $ENV{CFLAGS} , ${PROJECT_NAME}")

# 指定编译类型为 DEBUG
SET(CMAKE_BUILD_TYPE DEBUG)
# 指定编译参数
SET(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} ${SHOBJ_CFLAGS}")

# 指定编译类型为RELEASE
#SET(CMAKE_BUILD_TYPE RELEASE)
# 指定编译参数
#SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

######################生成可执行文件##########################

#设置需要链接的库目录
#LINK_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/linux64)

#设置所有需要链接的库名到 LIBRARIES 环境变量
#SET(LIBRARIES libbson-1.0.so libmongoc-1.0.so pthread z m iconv charset)

#生成可执行文件
#ADD_EXECUTABLE(../../bin/xxx ${SRC_LIST})

#把LIBRARIES链接到输出文件中
#TARGET_LINK_LIBRARIES(../../bin/xxx ${LIBRARIES})

###########################################################

######################生成链接库文件##########################
#设置需要链接的库目录
LINK_DIRECTORIES(${CMAKE_SOURCE_DIR}/libuv)

#生成动态库目标
ADD_LIBRARY(${PROJECT_NAME} SHARED ${SRC_LIST})

# 生成静态库目标
#SET(STATIC_NAME ${PROJECT_NAME}_A)
#ADD_LIBRARY(${STATIC_NAME} STATIC ${SRC_LIST})

#设置链接库的环境变量
SET(LIBRARIES rt m uv.a pthread)
# 把LIBRARIES链接到输出文件中,需在ADD_LIBRARY之后定义
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${LIBRARIES})

# 指定静态库的输出名称
#SET_TARGET_PROPERTIES(${STATIC_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
#检查名字是否切换过来了
#GET_TARGET_PROPERTY(OUTPUT_VALUE ${STATIC_NAME} OUTPUT_NAME)
#MESSAGE (STATUS ${STATIC_NAME} " ==> OUTPUT_NAME: " ${OUTPUT_VALUE})

# 使动态库和静态库同时存在
#SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES CLEAN_DIRECT_OUTPUT 1)
#SET_TARGET_PROPERTIES(${STATIC_NAME} PROPERTIES CLEAN_DIRECT_OUTPUT 1)

# 指定动态库版本  VERSION:动态库版本 SOVERSION:API版本
#SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES VERSION 1.2 SOVERSION 1)

###########################################################
