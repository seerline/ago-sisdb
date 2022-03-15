#定义工程名,必须在第一行
PROJECT(api_dms)
#检查版本 必须大于等于2.6版本
CMAKE_MINIMUM_REQUIRED(VERSION 2.8) 

SET(MYMODE "mode_${MODE}")
IF (MYMODE STREQUAL "mode_")
    MESSAGE("need set MODE." ${MYMODE})
ELSEIF (MYMODE STREQUAL "mode_TEST")
    SET(PROJECT_NAME api_dms)
    ADD_DEFINITIONS(-DTEST_DEBUG)
ELSE ()
    SET(PROJECT_NAME api_dms)
ENDIF ()
MESSAGE("MODE:" ${MYMODE})

#设置显示编译参数 方便验证编译参数
SET(CMAKE_VERBOSE_MAKEFILE 1)

########################运行平台判定和初始化######################

MESSAGE(STATUS "operation system is ${PROJECT} ${CMAKE_SYSTEM}")
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    # SET(SHOBJ_CFLAGS "-Wall -g -ggdb -std=gnu99 -O2")
    SET(SHOBJ_CFLAGS "-fPIC -Wall -Wunused-value -Wno-deprecated -Wno-format-truncation -Wno-format-overflow -g -O3 -std=gnu99")
    SET(SHOBJ_CXXFLAGS "-fPIC -Wall -Wunused-value -Wno-deprecated -Wno-format-truncation  -Wno-format-overflow -g -O3 -std=c++11")
    MESSAGE(STATUS "current platform: Linux ")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    SET(SHOBJ_CFLAGS ${CMAKE_C_FLAGS_DEBUG})
    SET(SHOBJ_CXXFLAGS ${CMAKE_CXX_FLAGS_DEBUG})
    # 设置为不依赖vc库编译
    SET(CMAKE_CXX_FLAGS_RELEASE "/MT /O2 /Ob2 /DNDEBUG")
    # SET(CMAKE_CXX_FLAGS_DEBUG "/MTd /Zi /Ob0 /Od /RTC1")
    SET(CMAKE_CXX_FLAGS_DEBUG "/MT /Zi /Ob0 /Od /RTC1")
    MESSAGE(STATUS "current platform: Windows")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    ADD_DEFINITIONS(-DENV_MACOSX)
    SET(CMAKE_MACOSX_RPATH 1)
    SET(SHOBJ_CFLAGS "-Wall -dynamic -fno-common -g -ggdb -std=gnu99 -O2")
    SET(SHOBJ_CXXFLAGS "-Wall -dynamic -fno-common -g -ggdb -std=c++11 -O2")
    MESSAGE(STATUS "current platform: Darwin")
ELSE ()
    MESSAGE(FATAL_ERROR "other platform: ${CMAKE_SYSTEM_NAME}, error!")
ENDIF ()

#检查是否为64位操作系统
IF (CMAKE_SIZEOF_VOID_P MATCHES 4)
  MESSAGE(FATAL_ERROR "operation system is 32, fail!")
ENDIF ()

#输出默认编译、链接信息
MESSAGE(STATUS "CMAKE_CXX_FLAGS = " ${CMAKE_CXX_FLAGS})
MESSAGE(STATUS "CMAKE_CXX_FLAGS_DEBUG = " ${CMAKE_CXX_FLAGS_DEBUG})
MESSAGE(STATUS "CMAKE_CXX_FLAGS_RELEASE = " ${CMAKE_CXX_FLAGS_RELEASE})

#########################设置颜色定义#############################
IF(NOT WIN32)
  STRING(ASCII 27 ESC)
  SET(CLR_RESET "${ESC}[m")
  SET(CLR_WARN  "${ESC}[1;35m")
ENDIF()

#########################基础目录设置#############################
MESSAGE(STATUS "SET DIR : ${CMAKE_SOURCE_DIR}")
SET(MYSRC_PATH ${CMAKE_SOURCE_DIR}/)
SET(SISDB_PATH ${CMAKE_SOURCE_DIR}/../../../src/)
#########################设置需要忽略的目录#############################
LIST(APPEND IGNOREPATH ".DS_Store")
LIST(APPEND IGNOREPATH "o")
IF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # LIST(APPEND IGNOREPATH "market_api_wb")
    # LIST(APPEND IGNOREPATH "market_api_wbtop")
ENDIF ()    
# LIST(APPEND IGNOREPATH "source_wind")
# LIST(APPEND IGNOREPATH "source_zjfast")

# make modules control file
FUNCTION(MAKE_CONTROL_FILE SEARCH_PATH MARK IN_LIST MODULE_LIST MODULE_PATHLIST)
    LIST(APPEND FILETYPE LIST_DIRECTORIES TRUE "${SEARCH_PATH}*")
    FILE(GLOB DIRS ${FILETYPE}) 
    FOREACH (DIR ${DIRS})
        IF (IS_DIRECTORY ${DIR})
            # 获取文件名称
            STRING(REGEX REPLACE ".+/(.+)\\.*" "\\1" FILE_NAME ${DIR})
            LIST(FIND IGNOREPATH ${FILE_NAME} ISREPEAT)
            IF (ISREPEAT MATCHES -1)
                LIST(APPEND RPLACE_LIST "${FILE_NAME}")  
            ENDIF ()  
        ENDIF ()        
    ENDFOREACH(DIR)
    # MESSAGE("show 1:" ${RPLACE_LIST})
    IF (RPLACE_LIST STREQUAL "")
        RETURN ()
    ENDIF ()

    # 判断有没有重复的类名
    SET(CLASS_LIST ${IN_LIST})
    FOREACH (DIR ${RPLACE_LIST})
        IF (IS_DIRECTORY ${SEARCH_PATH}${DIR})
            LIST(FIND CLASS_LIST ${DIR} ISREPEAT)
            IF (ISREPEAT MATCHES -1)
                LIST(APPEND CLASS_LIST "${DIR}")
                LIST(APPEND CUR_PATHLIST "${SEARCH_PATH}${DIR}/")
                # MESSAGE("search ${MARK} ${DIR} ok ...")
            ELSE ()
                MESSAGE(FATAL_ERROR "${CLR_WARN}*** ${MARK} name [ ${DIR} ] already exists! ***${CLR_RESET}")            
            ENDIF ()
            # ELSE ()
            #     MESSAGE(STATUS "${CLR_WARN}*** ${MARK} name [ ${DIR} ] remove! ***${CLR_RESET}")            
            # ENDIF ()
        ENDIF ()
    ENDFOREACH(DIR)
    SET(MODULE_LIST ${CLASS_LIST} PARENT_SCOPE)
    SET(MODULE_PATHLIST ${CUR_PATHLIST} PARENT_SCOPE)
ENDFUNCTION(MAKE_CONTROL_FILE)

FUNCTION(WRITE_CONTROL_FILE IN_LIST)
    SET(SEARCH_PATH ${SISDB_PATH})
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
    LIST(APPEND FILETYPE LIST_DIRECTORIES TRUE "${SEARCH_PATH}*")
    FILE(GLOB DIRS ${FILETYPE}) 
    FOREACH (DIR ${DIRS})
        IF (IS_DIRECTORY ${DIR})
            # 获取文件名称
            STRING(REGEX REPLACE ".+/(.+)\\.*" "\\1" FILE_NAME ${DIR})
            LIST(FIND IGNOREPATH ${FILE_NAME} ISREPEAT)
            IF (ISREPEAT MATCHES -1)
                LIST(APPEND RPLACE_LIST "${FILE_NAME}")  
            ENDIF ()  
        ENDIF ()        
    ENDFOREACH(DIR)
    # MESSAGE("show 1:" ${RPLACE_LIST})
    IF (RPLACE_LIST STREQUAL "")
        RETURN ()
    ENDIF ()

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
    # MESSAGE("---:" ${FILE_LIST}})
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
        # MESSAGE("OUT HEAD: ${FILE_N} -> ${FILE_PATH}")
        LIST(FIND PATH_LIST ${FILE_PATH} ISREPEAT)
        IF (ISREPEAT MATCHES -1)
            LIST(APPEND PATH_LIST "${FILE_PATH}")
        ENDIF ()
    ENDFOREACH(FILE_N)
    SET(SRC_PATHS ${PATH_LIST} PARENT_SCOPE)
ENDFUNCTION(FIND_SRCHEAD)

# depth search .a
FUNCTION(FIND_SRCLIB SEARCH_PATH SRC_PATHS SRC_FILES)
    IF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
        LIST(APPEND FILETYPE "${SEARCH_PATH}/*.a")
        LIST(APPEND FILETYPE "${SEARCH_PATH}/*.dylib")
    ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
        LIST(APPEND FILETYPE "${SEARCH_PATH}/*_a.lib")
    ELSE ()
        LIST(APPEND FILETYPE "${SEARCH_PATH}/*.a")
        LIST(APPEND FILETYPE "${SEARCH_PATH}/*.so")
    ENDIF ()
    FILE(GLOB_RECURSE FILE_LIST ${FILETYPE}) 

    FOREACH(FILE_N ${FILE_LIST})
        STRING(REGEX REPLACE ".+/(.+)\\.*"  "\\1" FILE_NAME ${FILE_N})
        LIST(FIND LIB_LIST ${FILE_NAME} ISREPEAT)
        IF (ISREPEAT MATCHES -1)
            LIST(APPEND LIB_LIST "${FILE_NAME}")
        ENDIF ()
    ENDFOREACH(FILE_N)
    SET(SRC_FILES ${LIB_LIST} PARENT_SCOPE)

    FOREACH(PATH_N ${FILE_LIST})
        STRING(REGEX REPLACE "(.+)\\/.*" "\\1" FILE_PATH ${PATH_N})
        LIST(FIND PATH_LIST ${FILE_PATH} ISREPEAT)
        IF (ISREPEAT MATCHES -1)
            LIST(APPEND PATH_LIST "${FILE_PATH}")
        ENDIF ()
    ENDFOREACH(PATH_N)
    SET(SRC_PATHS ${PATH_LIST} PARENT_SCOPE)
ENDFUNCTION(FIND_SRCLIB)

# 由于不同的库可能冲突 需要编译的是否带参数

MAKE_CONTROL_FILE("${MYSRC_PATH}../../drives-market/" "drives-market" "${MODULE_LIST}" MODULE_LIST MODULE_PATHLIST)
LIST(APPEND SEARCHPATH_LIST "${MODULE_PATHLIST}")
MAKE_CONTROL_FILE("${SISDB_PATH}../workers/" "workers" "${MODULE_LIST}" MODULE_LIST MODULE_PATHLIST)
LIST(APPEND SEARCHPATH_LIST "${MODULE_PATHLIST}")

# MESSAGE("OUT MODULES:" ${MODULE_LIST})  
# 写插件控制文件
WRITE_CONTROL_FILE("${MODULE_LIST}")

#########################目录设置#############################

IF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    SET(SISDB_SDK_PATH ${SISDB_PATH}../sdk/macos/)
    SET(SISDB_SYSTEM_PATH ${SISDB_PATH}os/linux/)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    SET(SISDB_SDK_PATH ${SISDB_PATH}../sdk/win32/)
    SET(SISDB_SYSTEM_PATH ${SISDB_PATH}os/win32/)
ELSE ()
    SET(SISDB_SDK_PATH ${SISDB_PATH}../sdk/linux/)
    SET(SISDB_SYSTEM_PATH ${SISDB_PATH}os/linux/)
ENDIF ()

#设置头文件目录
INCLUDE_DIRECTORIES(${MYSRC_PATH}/../../src)
INCLUDE_DIRECTORIES(${MYSRC_PATH})
INCLUDE_DIRECTORIES(${SISDB_PATH})
INCLUDE_DIRECTORIES(${SISDB_SYSTEM_PATH})

#设置基础源文件目录
AUX_SOURCE_DIRECTORY(${MYSRC_PATH}/../../src CUR_FILE)
LIST(APPEND SRCFILE_LIST ${CUR_FILE})
AUX_SOURCE_DIRECTORY(${MYSRC_PATH} CUR_FILE)
LIST(APPEND SRCFILE_LIST ${CUR_FILE})
AUX_SOURCE_DIRECTORY(${SISDB_PATH} CUR_FILE)
LIST(APPEND SRCFILE_LIST ${CUR_FILE})
AUX_SOURCE_DIRECTORY(${SISDB_SYSTEM_PATH} CUR_FILE)
LIST(APPEND SRCFILE_LIST ${CUR_FILE})

# sdk only install include & lib, exclude cpp
FIND_SDKPATH("${SISDB_SDK_PATH}" SDK_PATHS)

FOREACH(SRC_PATH ${SDK_PATHS})
    # MESSAGE("OUT SDKPATH:" ${SRC_PATH})  
    INCLUDE_DIRECTORIES("${SRC_PATH}/include/")
    FIND_SRCLIB(${SRC_PATH} SRC_PATHS SRC_FILES)
    SET(LIBPATH_LIST ${LIBPATH_LIST} ${SRC_PATHS})
    SET(LIBFILE_LIST ${LIBFILE_LIST} ${SRC_FILES})
    # MESSAGE("OUT LIBPATH:" ${LIBPATH_LIST})  
    # MESSAGE("OUT LIBFILE:" ${LIBFILE_LIST})  
ENDFOREACH(SRC_PATH)

LIST(APPEND SEARCHPATH_LIST "${SISDB_PATH}core/")

FOREACH(SEARCHPATH ${SEARCHPATH_LIST})
    FIND_SRCFILE(${SEARCHPATH} SRC_FILES)
    SET(SRCFILE_LIST ${SRCFILE_LIST} ${SRC_FILES})
    # MESSAGE("SRC FILES:" ${SEARCHPATH})

    FIND_SRCHEAD(${SEARCHPATH} SRC_PATHS)
    # MESSAGE("HEAD PATHS:" ${SRC_PATHS})
    FOREACH(SRC_PATH ${SRC_PATHS})
        # MESSAGE("HEAD PATH:" ${SRC_PATH})
        INCLUDE_DIRECTORIES(${SRC_PATHS})
    ENDFOREACH(SRC_PATH)

    FIND_SRCLIB(${SEARCHPATH} SRC_PATHS SRC_FILES)
    # 增加不重复的路径
    FOREACH(PATH_N ${SRC_PATHS})
        LIST(FIND LIBPATH_LIST ${PATH_N} ISREPEAT)
        IF (ISREPEAT MATCHES -1)
            # MESSAGE("ADD PATHS:" ${PATH_N})
            LIST(APPEND LIBPATH_LIST "${PATH_N}")
        ENDIF ()
    ENDFOREACH(PATH_N)
    # 增加不重复的文件
    FOREACH(FILE_N ${SRC_FILES})
        LIST(FIND LIBFILE_LIST ${FILE_N} ISREPEAT)
        IF (ISREPEAT MATCHES -1)
            # MESSAGE("ADD PATHS:" ${FILE_N})
            LIST(APPEND LIBFILE_LIST "${FILE_N}")
        ENDIF ()
    ENDFOREACH(FILE_N)
    # MESSAGE("LIBS PATHS:" ${SRC_PATHS})
    # MESSAGE("LIBS FILES:" ${SRC_FILES})
ENDFOREACH(SEARCHPATH)

# FOREACH(MSG ${SRCFILE_LIST})
#     MESSAGE("SRC FILES:" ${MSG})
# ENDFOREACH(MSG)    
FOREACH(MSG ${LIBPATH_LIST})
    MESSAGE("LIBS PATHS:" ${MSG})
ENDFOREACH(MSG)    
FOREACH(MSG ${LIBFILE_LIST})
    MESSAGE("LIBS FILES:" ${MSG})
ENDFOREACH(MSG)    

########################设置编译类型和参数#####################
# 指定编译器
IF(MSVC)
ELSE()
    SET(CMAKE_C_COMPILER "/usr/bin/gcc")
    SET(CMAKE_CXX_COMPILER "/usr/bin/g++")
ENDIF()
#输出信息
MESSAGE(STATUS "FLAGS : $ENV{CFLAGS} ${SHOBJ_CFLAGS} --> ${PROJECT_NAME}")

# 设置预定义变量
IF(MSVC)
	# add_definitions("-DPT_TradeDataAPI_EXPORTS")
    SET(CMAKE_EXE_LINKER_FLAGS /NODEFAULTLIB:"libcmt.lib")
ENDIF()

# 指定编译类型为 DEBUG
SET(CMAKE_BUILD_TYPE DEBUG) 
# 指定编译类型为RELEASE
# SET(CMAKE_BUILD_TYPE RELEASE) 

# 指定编译参数
SET(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} ${SHOBJ_CFLAGS}")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} ${SHOBJ_CXXFLAGS}")

#设置需要链接的库目录
LINK_DIRECTORIES(${LIBPATH_LIST})

#设置链接库的环境变量
IF(MSVC)
    #设置需要链接的库目录
	SET(LIBRARIES
    iphlpapi.lib
    userenv.lib
    psapi.lib
    ${LIBFILE_LIST}
	)
ELSE()
    SET(LIBRARIES ${LIBFILE_LIST} c m stdc++ dl pthread)
ENDIF()
######################目录设置##########################
# # 以下设置VS会自动新建Debug和Release文件夹
# set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Lib)
# set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Lib)
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Bin)
# # 以下设置分别设置Debug和Release输出目录
# set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Lib)
# set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Lib)
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Bin)
# set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Lib)
# set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Lib)
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Bin)
######################输出文件##########################
SET(MYOUT_PATH ${CMAKE_SOURCE_DIR}/../lib)

# cmake .. -DMODE=static 加参数运行时 生成静态链接库
# cmake .. -DMODE=share  加参数运行时 生成动态链接库
SET(MYMODE "compiler_${MODE}")
MESSAGE(STATUS "compiler : ${MYMODE}")

IF (MYMODE STREQUAL "compiler_static")
    ######################生成静态库文件##########################
    # 设置输出库文件目录
    MESSAGE(STATUS "static :  ${MYMODE}" )
    IF(MSVC)
        SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${MYOUT_PATH})
    ELSE()
        SET(LIBRARY_OUTPUT_PATH ${MYOUT_PATH}) 
    ENDIF()
    ADD_LIBRARY(${PROJECT_NAME} STATIC ${SRCFILE_LIST})
    # IF(MSVC AND MSVC_STATIC_RUNTIME)     
    #     FOREACH(var 
    #         CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
    #         CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
    #     )
    #     IF(${var} STREQUAL "/MD")
    #         STRING(REGEX REPLACE "/MD" "/MT" ${var} "${${var}}")
    #     ENDIF()
    #     ENDFOREACH()
    # ENDIF()
    # 把LIBRARIES链接到输出文件中,需在ADD_LIBRARY之后定义
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${LIBRARIES}) 
    # 去除目标生成的Lib前缀
    # SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES PREFIX "")
ELSEIF (MYMODE STREQUAL "compiler_share")
    ######################生成动态库文件##########################
    # 设置输出库文件目录
    MESSAGE(STATUS "share : ${MYMODE}")
    IF(MSVC)
        SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${MYOUT_PATH})
    ELSE()
        SET(LIBRARY_OUTPUT_PATH ${MYOUT_PATH}) 
    ENDIF()
    ADD_LIBRARY(${PROJECT_NAME} SHARED ${SRCFILE_LIST})
    # 把LIBRARIES链接到输出文件中,需在ADD_LIBRARY之后定义
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${LIBRARIES}) 
    # 去除目标生成的.so前缀
    # SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES SUFFIX "")
    IF(MSVC)
        SET_TARGET_PROPERTIES( ${PROJECT_NAME} PROPERTIES SUFFIX ".dll")
    ENDIF()
ELSE()
    ######################默认生成可执行文件文件##########################
    #设置目标文件的目录
    MESSAGE(STATUS "exec : ${MYMODE}")
    SET(EXECUTABLE_OUTPUT_PATH ${MYOUT_PATH})
    SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${MYOUT_PATH})
    #生成可执行文件
    ADD_EXECUTABLE(${PROJECT_NAME} ${SRCFILE_LIST})
    #把LIBRARIES链接到输出文件中
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${LIBRARIES})
ENDIF()