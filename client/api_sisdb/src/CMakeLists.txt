#定义工程名,必须在第一行
PROJECT(api_sisdb)
#检查版本 必须大于等于2.6版本
CMAKE_MINIMUM_REQUIRED(VERSION 2.6) 
#设置显示编译参数 方便验证编译参数
SET(CMAKE_VERBOSE_MAKEFILE 1)

# #为用户提供一个可选项:设置变量初始化值为ON，在cmake-gui构建项目可设置
# option(BUILD_SHARED_LIBS "Build Shared Libraries" ON)
# option(BUILD_TYPE_DEFAULT_DEBUG "Build-type Debug" ON)
# option(MSVC_STATIC_RUNTIME "Link static runtime libraries" ON)

SET(BUILD_USE_64BITS ON)
########################运行平台判定和初始化######################
MESSAGE(STATUS "operation system is ${PROJECT} ${CMAKE_SYSTEM}")
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    SET(SHOBJ_CFLAGS "-fPIC -Wall -Wunused-value -Wno-deprecated -g -O3 -std=gnu99")
    SET(SHOBJ_CXXFLAGS "-fPIC -Wall -Wunused-value -Wno-deprecated -g -O3 -std=c++11")
    MESSAGE(STATUS "current platform: Linux ")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
	SET(SHOBJ_CFLAGS ${CMAKE_C_FLAGS_DEBUG})
    SET(SHOBJ_CXXFLAGS ${CMAKE_CXX_FLAGS_DEBUG})
    # 设置为不依赖vc库编译
    SET(CMAKE_CXX_FLAGS_RELEASE "/MT /Od /Zp1 /Ob2 /DNDEBUG")
    SET(CMAKE_CXX_FLAGS_DEBUG "/MTd /Zi /Zp1 /Ob0 /Od /RTC1")
    MESSAGE(STATUS "current platform: Windows")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    MESSAGE(STATUS "current platform: Darwin")
    ADD_DEFINITIONS(-DENV_MACOSX)
    SET(CMAKE_MACOSX_RPATH 1)
    SET(SHOBJ_CFLAGS "-Wall -dynamic -fno-common -g -ggdb -std=gnu99 -O2")
    SET(SHOBJ_CXXFLAGS "-Wall -dynamic -fno-common -g -ggdb -std=c++11 -O2")
ELSE ()
    MESSAGE(STATUS "other platform: ${CMAKE_SYSTEM_NAME}, error!")
ENDIF ()

#检查是否为64位操作系统
IF (CMAKE_SIZEOF_VOID_P MATCHES 4)
  MESSAGE(STATUS "operation system is 32, error!")
ENDIF ()

#输出默认编译、链接信息
MESSAGE(STATUS "CMAKE_CXX_FLAGS = " ${CMAKE_CXX_FLAGS})
MESSAGE(STATUS "CMAKE_CXX_FLAGS_DEBUG = " ${CMAKE_CXX_FLAGS_DEBUG})
MESSAGE(STATUS "CMAKE_CXX_FLAGS_RELEASE = " ${CMAKE_CXX_FLAGS_RELEASE})

#########################目录设置#############################
SET(SISLIB_PATH ${CMAKE_SOURCE_DIR}/../../../src/)
SET(STKLIB_PATH ${CMAKE_SOURCE_DIR}/../../../)

#设置头文件目录
INCLUDE_DIRECTORIES(
    ${STKLIB_PATH}src/
    ${SISLIB_PATH}
    ${SISLIB_PATH}core/
    ${SISLIB_PATH}core/disk
    ${SISLIB_PATH}core/net
    ${SISLIB_PATH}workers/sisdb_client/
    ${SISLIB_PATH}workers/sisdb/
	${SISLIB_PATH}../sdk/libuv/include
	${SISLIB_PATH}../sdk/snappy/include
    .)
IF(MSVC)
	INCLUDE_DIRECTORIES(${SISLIB_PATH}os/win32)
ELSE()
	INCLUDE_DIRECTORIES(${SISLIB_PATH}os/linux)
ENDIF()
			
#提取源目录下所有文件
AUX_SOURCE_DIRECTORY(. DIR_SRCS)
AUX_SOURCE_DIRECTORY(${STKLIB_PATH}src/ DIR_STK_MAIN)
AUX_SOURCE_DIRECTORY(${SISLIB_PATH} DIR_MAIN)
AUX_SOURCE_DIRECTORY(${SISLIB_PATH}core/ DIR_CORE)
AUX_SOURCE_DIRECTORY(${SISLIB_PATH}core/net DIR_CORE_NET)
AUX_SOURCE_DIRECTORY(${SISLIB_PATH}core/disk DIR_CORE_DISK)
AUX_SOURCE_DIRECTORY(${SISLIB_PATH}workers/sisdb/ DIR_SISDB)
AUX_SOURCE_DIRECTORY(${SISLIB_PATH}workers/sisdb_client/ DIR_CLIENT)
IF(MSVC)
	AUX_SOURCE_DIRECTORY(${SISLIB_PATH}os/win32/ DIR_CORE_OS)
ELSE()
	AUX_SOURCE_DIRECTORY(${SISLIB_PATH}os/linux/ DIR_CORE_OS)
ENDIF()

#把源文件目录设置到环境变量
SET(SRC_LIST ${DIR_SRCS}
             ${DIR_STK_MAIN}
             ${DIR_CORE}
             ${DIR_CORE_NET}
             ${DIR_CORE_DISK}
             ${DIR_MAIN}
             ${DIR_SISDB}
			 ${DIR_CLIENT}
			 ${DIR_CORE_OS}
             )

MESSAGE(STATUS "list : ${SRC_LIST}")
########################设置编译类型和参数#####################
SET(CMAKE_C_COMPILER "/usr/bin/gcc")
SET(CMAKE_CXX_COMPILER "/usr/bin/g++")

#输出信息
MESSAGE(STATUS "FLAGS : ${PROJECT_NAME}")

# 设置预定义变量
IF(MSVC)
	# add_definitions("-DPT_TradeDataAPI_EXPORTS")
ENDIF()

# 指定编译类型为 DEBUG 默认debug
SET(CMAKE_BUILD_TYPE DEBUG) 
# SET(CMAKE_BUILD_TYPE RELEASE) 

# 指定编译参数
SET(CMAKE_C_FLAGS_DEBUG "${SHOBJ_CFLAGS}")
SET(CMAKE_CXX_FLAGS_DEBUG "${SHOBJ_CXXFLAGS}")

######################生成可执行文件文件##########################
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../)
# 一旦设置下面的属性,在任何环境下生成的可执行文件都将直接放在你所设置的目录 
# SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_SOURCE_DIR}/../bin)
# SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_SOURCE_DIR}/../bin)
#设置需要链接的库目录
LINK_DIRECTORIES(
	${SISLIB_PATH}../sdk/libuv/lib
	${SISLIB_PATH}../sdk/snappy/lib
	# ${CMAKE_SOURCE_DIR}/../../../linux/lib/python3.5
    )
#设置所有需要链接的库名到 LIBRARIES 环境变量
MESSAGE(STATUS "LIBRARIES : ${LIBFILE_LIST}")

IF(MSVC)
	SET(LIBRARIES
        # python35
        uv 
        snappy64
        # uv_a.lib
        # snappy_a.lib
	)
ELSE()
	SET(LIBRARIES libuv.a libsnappy.a pthread stdc++
		# ${MY_LIB_PATH}python3.5/libboost_python3.a
		# ${MY_LIB_PATH}libboost.a
	)
ENDIF()
# SET(LIBRARIES uv snappy pthread stdc++)
#生成可执行文件
ADD_EXECUTABLE(${PROJECT_NAME} ${SRC_LIST})

#把LIBRARIES链接到输出文件中
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${LIBRARIES})

######################生成静态链接库文件##########################
# 设置输出库文件目录
# IF(MSVC)
#     SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ../../lib/)
#     SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ../../lib/)
# ELSE()
#     SET(LIBRARY_OUTPUT_PATH ../../lib/) 
# ENDIF()

# #设置链接库目录
# LINK_DIRECTORIES(
# 	${SISLIB_PATH}../sdk/libuv/lib
# 	${SISLIB_PATH}../sdk/snappy/lib
# 	# ${CMAKE_SOURCE_DIR}/../../../linux/lib/python3.5
#     )

# # 生成动态库
# ADD_LIBRARY(${PROJECT_NAME} SHARED ${SRC_LIST})

# # 生成静态库目标
# # SET(STATIC_NAME ${PROJECT_NAME})
# # ADD_LIBRARY(${PROJECT_NAME} STATIC ${SRC_LIST})
# # IF(MSVC AND MSVC_STATIC_RUNTIME)     
# #     foreach(var 
# #         CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
# #         CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
# #     )
# #     IF(${var} MATCHES "/MD")
# #         string(REGEX REPLACE "/MD" "/MT" ${var} "${${var}}")
# #     ENDIF()
# #     endforeach()
# # ENDIF()


# # SET(MY_LIB_PATH ${CMAKE_SOURCE_DIR}/../../../linux/lib/)  
# #设置链接库的环境变量
# IF(MSVC)
# 	SET(LIBRARIES
#         # python35
#         uv 
#         snappy64
#         # uv_a.lib
#         # snappy_a.lib
# 	)
# ELSE()
#     # SET(LIBRARIES pthread
# 	SET(LIBRARIES libuv.a libsnappy.a pthread stdc++
# 		# ${MY_LIB_PATH}python3.5/libboost_python3.a
# 		# ${MY_LIB_PATH}libboost.a
# 	)
# ENDIF()

# # 把LIBRARIES链接到输出文件中,需在ADD_LIBRARY之后定义
# TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${LIBRARIES}) 
# # 去除目标生成的Lib前缀
# SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES PREFIX "")
# # 去除目标生成的.so前缀
# #SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES SUFFIX "")

# IF(MSVC)
# SET_TARGET_PROPERTIES( ${PROJECT_NAME} PROPERTIES SUFFIX ".dll")
# ENDIF()

###########################################################

