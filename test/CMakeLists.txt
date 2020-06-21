#定义工程名,必须在第一行
PROJECT(test)

#检查版本 必须大于等于2.6版本
CMAKE_MINIMUM_REQUIRED(VERSION 2.8) 

#设置显示编译参数 方便验证编译参数
SET(CMAKE_VERBOSE_MAKEFILE 1)

########################运行平台判定和初始化######################

MESSAGE(STATUS "operation system is ${PROJECT} ${CMAKE_SYSTEM}")
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    SET(SHOBJ_CFLAGS "-Wall -g -ggdb -std=gnu99 -O2")
    MESSAGE(STATUS "current platform: Linux ")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    MESSAGE(STATUS "current platform: Windows")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    MESSAGE(STATUS "current platform: Darwin")
    SET(SHOBJ_CFLAGS -Wall -dynamic -fno-common -g -ggdb -std=gnu99 -O2)
ELSE ()
    MESSAGE(FATAL_ERROR "other platform: ${CMAKE_SYSTEM_NAME}, error!")
ENDIF ()

#检查是否为64位操作系统
IF (CMAKE_SIZEOF_VOID_P MATCHES 4)
  MESSAGE(FATAL_ERROR "operation system is 32, fail!")
ENDIF ()

IF(NOT WIN32)
  STRING(ASCII 27 ESC)
  SET(CLR_RESET "${ESC}[m")
  SET(CLR_WARN  "${ESC}[1;35m")
ENDIF()

#########################基础目录设置#############################
SET(SISDB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src/")
#########################自动目录设置#############################

# make modules control file
FUNCTION(MAKE_CONTROL_FILE SEARCH_PATH MARK IN_LIST OUT_LIST)
    # EXECUTE_PROCESS(COMMAND pwd ${SEARCH_PATH} OUTPUT_VARIABLE DIRS)
    # MESSAGE(${DIRS})
    EXECUTE_PROCESS(COMMAND ls ${SEARCH_PATH} OUTPUT_VARIABLE DIRS)

    IF (DIRS STREQUAL "")
        RETURN ()
    ENDIF ()
    STRING(REPLACE "\n" ";" RPLACE_LIST ${DIRS})
    # 判断有没有重复的类名
    SET(CLASS_LIST ${IN_LIST})
    FOREACH (DIR ${RPLACE_LIST})
        IF (IS_DIRECTORY ${SEARCH_PATH}${DIR})
            LIST(FIND CLASS_LIST ${DIR} ISREPEAT)
            IF (ISREPEAT MATCHES -1)
                LIST(APPEND CLASS_LIST "${DIR}")
                # MESSAGE("search ${MARK} ${DIR} ok ...")
            ELSE ()
                MESSAGE(FATAL_ERROR "${CLR_WARN}*** ${MARK} name [ ${DIR} ] already exists! ***${CLR_RESET}")            
            ENDIF ()
        ENDIF ()
    ENDFOREACH(DIR)
    SET(OUT_LIST ${CLASS_LIST} PARENT_SCOPE)
ENDFUNCTION(MAKE_CONTROL_FILE)

FUNCTION(WRITE_CONTROL_FILE IN_LIST)
    SET(SEARCH_PATH ${SISDB_SOURCE_DIR})
    SET(FILENAME "${SEARCH_PATH}modules.c")
    FILE(WRITE ${FILENAME} "#include <sis_modules.h>\n\n" )
    SET(CLASS_LIST ${IN_LIST})
    # 写类引用
    FOREACH (DIR ${CLASS_LIST})
        MESSAGE("install ${DIR} ok ...")
        FILE(APPEND ${FILENAME} "extern s_sis_modules sis_modules_${DIR};\n")
    ENDFOREACH(DIR)
    # 写类数组
    FILE(APPEND ${FILENAME} "\ns_sis_modules *__modules[] = {\n")
    FOREACH (DIR ${CLASS_LIST})
        FILE(APPEND ${FILENAME} "    &sis_modules_${DIR},\n")
    ENDFOREACH(DIR)
    FILE(APPEND ${FILENAME} "    0\n  };\n")
    # 写类名
    FILE(APPEND ${FILENAME} "\nconst char *__modules_name[] = {\n")
    FOREACH (DIR ${CLASS_LIST})
        FILE(APPEND ${FILENAME} "    \"${DIR}\",\n")
    ENDFOREACH(DIR)
    FILE(APPEND ${FILENAME} "    0\n  };\n")
ENDFUNCTION(WRITE_CONTROL_FILE)

# search sdk
FUNCTION(FIND_SDKPATH SEARCH_PATH SDK_PATHS)
    EXECUTE_PROCESS(COMMAND ls ${SEARCH_PATH} OUTPUT_VARIABLE DIRS)
    STRING(REPLACE "\n" ";" RPLACE_LIST ${DIRS})

    FOREACH (DIR ${RPLACE_LIST})
        IF (IS_DIRECTORY ${SEARCH_PATH}${DIR})
            LIST(APPEND PATH_LIST "${SEARCH_PATH}${DIR}")
        ENDIF ()
    ENDFOREACH(DIR)
    # MESSAGE("PATH_LIST :" ${PATH_LIST})
    SET(SDK_PATHS ${PATH_LIST} PARENT_SCOPE)
ENDFUNCTION(FIND_SDKPATH)

# depth search c cpp
FUNCTION(FIND_SRCFILE SEARCH_PATH SRC_FILES)
    # 不找子目录
    # FILE(GLOB FILE_LIST ${SEARCH_PATH}) 
    LIST(APPEND FILETYPE "${SEARCH_PATH}*.c")
    LIST(APPEND FILETYPE "${SEARCH_PATH}*.cpp")
    LIST(APPEND FILETYPE "${SEARCH_PATH}*.cc")
    FILE(GLOB_RECURSE FILE_LIST ${FILETYPE}) 
    # MESSAGE("---:" ${FILETYPE})
    SET(SRC_FILES ${FILE_LIST} PARENT_SCOPE)
ENDFUNCTION(FIND_SRCFILE)

# depth search headfile
FUNCTION(FIND_SRCHEAD SEARCH_PATH SRC_PATHS)
    LIST(APPEND FILETYPE "${SEARCH_PATH}*.h")
    LIST(APPEND FILETYPE "${SEARCH_PATH}*.hpp")
    FILE(GLOB_RECURSE FILE_LIST ${FILETYPE}) 
    # 获取文件名称
    # STRING(REGEX REPLACE ".+/(.+)\\.*" "\\1" FILE_NAME ${FILE_PATH})
    FOREACH(FILE_N ${FILE_LIST})
        STRING(REGEX REPLACE "(.+)\\/.*" "\\1" FILE_PATH ${FILE_N})
        LIST(FIND PATH_LIST ${FILE_PATH} ISREPEAT)
        IF (ISREPEAT MATCHES -1)
            LIST(APPEND PATH_LIST "${FILE_PATH}")
        ENDIF ()
    ENDFOREACH(FILE_N)
    SET(SRC_PATHS ${PATH_LIST} PARENT_SCOPE)
ENDFUNCTION(FIND_SRCHEAD)

# depth search .a
FUNCTION(FIND_SRCLIB SEARCH_PATH SRC_PATHS SRC_FILES)
    LIST(APPEND FILETYPE "${SEARCH_PATH}/*.a")
    FILE(GLOB_RECURSE FILE_LIST ${FILETYPE}) 

    FOREACH(FILE_N ${FILE_LIST})
        STRING(REGEX REPLACE ".+/(.+)\\.*"  "\\1" FILE_NAME ${FILE_N})
        LIST(FIND LIB_LIST ${FILE_NAME} ISREPEAT)
        IF (ISREPEAT MATCHES -1)
            LIST(APPEND LIB_LIST "${FILE_NAME}")
        ENDIF ()
    ENDFOREACH(FILE_N)
    SET(SRC_FILES ${LIB_LIST} PARENT_SCOPE)

    FOREACH(FILE_N ${FILE_LIST})
        STRING(REGEX REPLACE "(.+)\\/.*" "\\1" FILE_PATH ${FILE_N})
        LIST(FIND PATH_LIST ${FILE_PATH} ISREPEAT)
        IF (ISREPEAT MATCHES -1)
            LIST(APPEND PATH_LIST "${FILE_PATH}")
        ENDIF ()
    ENDFOREACH(FILE_N)
    SET(SRC_PATHS ${PATH_LIST} PARENT_SCOPE)
ENDFUNCTION(FIND_SRCLIB)

MAKE_CONTROL_FILE("${SISDB_SOURCE_DIR}/../modules/" "modules" "" OUT_LIST)
MAKE_CONTROL_FILE("${SISDB_SOURCE_DIR}/workers/" "workers" "${OUT_LIST}" OUT_LIST)

MESSAGE("OUT MODULES:" ${OUT_LIST})  
# 写插件控制文件
WRITE_CONTROL_FILE("${OUT_LIST}")

#########################基础目录设置#############################
#设置基础头文件目录

INCLUDE_DIRECTORIES(${SISDB_SOURCE_DIR})
INCLUDE_DIRECTORIES(${SISDB_SOURCE_DIR}os/linux)

#设置基础源文件目录
AUX_SOURCE_DIRECTORY(${SISDB_SOURCE_DIR} DIR_CUR)
AUX_SOURCE_DIRECTORY(${SISDB_SOURCE_DIR}os/linux DIR_SYSTEM)

#把基础源文件目录设置到环境变量
SET(SRCFILE_LIST ${DIR_CUR} ${DIR_SYSTEM})
# sdk only install include & lib, exclude cpp
FIND_SDKPATH("${SISDB_SOURCE_DIR}/../sdk/" SDK_PATHS)
FOREACH(SRC_PATH ${SDK_PATHS})
    INCLUDE_DIRECTORIES("${SRC_PATH}/include/")
    FIND_SRCLIB(${SRC_PATH} SRC_PATHS SRC_FILES)
    SET(LIBPATH_LIST ${LIBPATH_LIST} ${SRC_PATHS})
    SET(LIBFILE_LIST ${LIBFILE_LIST} ${SRC_FILES})
ENDFOREACH(SRC_PATH)

SET(SEARCHPATH_LIST 
    "${SISDB_SOURCE_DIR}/../modules/" 
    "${SISDB_SOURCE_DIR}/workers/" 
    "${SISDB_SOURCE_DIR}/core/"
    )

# MESSAGE("SRC FILES:" ${SRCFILE_LIST})    

FOREACH(SEARCHPATH ${SEARCHPATH_LIST})
    # 增加源文件
    FIND_SRCFILE(${SEARCHPATH} SRC_FILES)
    SET(SRCFILE_LIST ${SRCFILE_LIST} ${SRC_FILES})
    # MESSAGE("SRC FILES:" ${SRC_FILES})

    FIND_SRCHEAD(${SEARCHPATH} SRC_PATHS)
    # MESSAGE("HEAD PATHS:" ${SRC_PATHS})
    FOREACH(SRC_PATH ${SRC_PATHS})
        # MESSAGE("INCLUDE_DIR:" ${SRC_PATHS})
        INCLUDE_DIRECTORIES(${SRC_PATHS})
    ENDFOREACH(SRC_PATH)

    FIND_SRCLIB(${SEARCHPATH} SRC_PATHS SRC_FILES)
    # MESSAGE("LIBS PATHS:" ${SRC_PATHS})
    # MESSAGE("LIBS FILES:" ${SRC_FILES})
    SET(LIBPATH_LIST ${LIBPATH_LIST} ${SRC_PATHS})
    SET(LIBFILE_LIST ${LIBFILE_LIST} ${SRC_FILES})

ENDFOREACH(SEARCHPATH)

MESSAGE("SRC FILES:" ${SRCFILE_LIST})
MESSAGE("LIBS PATHS:" ${LIBPATH_LIST})
MESSAGE("LIBS FILES:" ${LIBFILE_LIST})


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
# 设置输出文件目录
SET(EXECUTABLE_OUTPUT_PATH ../)

#设置需要链接的库目录
LINK_DIRECTORIES(${LIBPATH_LIST})

#设置所有需要链接的库名到 LIBRARIES 环境变量
MESSAGE(STATUS "LIBRARIES : ${LIBFILE_LIST}")

# SET(LIBRARIES ${LIBFILE_LIST} c m iconv stdc++ pthread)
SET(LIBRARIES ${LIBFILE_LIST} c m stdc++ pthread)
# SET(LIBRARIES libuv.a libqs_api.a libboost.a c m stdc++ pthread)

#生成可执行文件
ADD_EXECUTABLE(${PROJECT_NAME} ${SRCFILE_LIST})

#把LIBRARIES链接到输出文件中
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${LIBRARIES})

###########################################################

######################生成链接库文件##########################
# 设置输出文件目录
# SET(LIBRARY_OUTPUT_PATH .)  

#生成动态库目标
#ADD_LIBRARY(${PROJECT_NAME} SHARED ${SRC_LIST})

# 生成静态库目标
#SET(STATIC_NAME ${PROJECT_NAME}_A)
#ADD_LIBRARY(${STATIC_NAME} STATIC ${SRC_LIST})

#设置链接库的环境变量
#SET(LIBRARIES c m)
# 把LIBRARIES链接到输出文件中,需在ADD_LIBRARY之后定义
#TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${LIBRARIES}) 

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

