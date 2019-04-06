#定义工程名,必须在第一行
PROJECT(sisdb)

#检查版本 必须大于等于2.6版本
CMAKE_MINIMUM_REQUIRED(VERSION 2.8) 

#设置显示编译参数 方便验证编译参数
SET(CMAKE_VERBOSE_MAKEFILE OFF)

########################运行平台判定和初始化######################

MESSAGE(STATUS "operation system is ${PROJECT} ${CMAKE_SYSTEM}")
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    SET(SHOBJ_CFLAGS "-Wall -g -ggdb -std=gnu99 -O2")
    MESSAGE(STATUS "current platform: Linux ")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    MESSAGE(STATUS "current platform: Windows")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    MESSAGE(STATUS "current platform: Darwin")
    SET(SHOBJ_CFLAGS -Wall -dl -dynamic -fno-common -g -ggdb -std=gnu99 -O2)
ELSE ()
    MESSAGE(STATUS "other platform: ${CMAKE_SYSTEM_NAME}, error!")
ENDIF ()

#检查是否为64位操作系统
IF (CMAKE_SIZEOF_VOID_P MATCHES 4)
  MESSAGE(STATUS "operation system is 32, error!")
ENDIF ()

#########################目录设置#############################

#设置头文件目录
INCLUDE_DIRECTORIES(
        ./server/src
        ./server/deps/jemalloc/include
        ./server/deps/lua/src
        ./server/deps/hiredis
        ./server/deps/linenoise
    )

#设置源文件目录
# AUX_SOURCE_DIRECTORY(./server/src DIR_SRCS)

# #把源文件目录设置到环境变量
# SET(SRC_LIST ${DIR_SRCS}
#              )

# 设置输出文件目录
SET(EXECUTABLE_OUTPUT_PATH ../../../bin/)
########################设置编译类型和参数#####################

#输出信息
MESSAGE(STATUS "FLAGS : $ENV{CFLAGS} , ${CMAKE_CURRENT_SOURCE_DIR}")

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
LINK_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/server/deps/jemalloc/lib/ 
    ${CMAKE_CURRENT_SOURCE_DIR}/server/deps/lua/src/
    ${CMAKE_CURRENT_SOURCE_DIR}/server/deps/hiredis/
    ${CMAKE_CURRENT_SOURCE_DIR}/server/deps/linenoise/)

#把源文件目录设置到环境变量

SET(SRC_SERVER 
            ./server/src/adlist.c
            ./server/src/quicklist.c
            ./server/src/ae.c
            ./server/src/anet.c
            ./server/src/dict.c
            ./server/src/sds.c
            ./server/src/zmalloc.c
            ./server/src/lzf_c.c
            ./server/src/lzf_d.c
            ./server/src/pqsort.c
            ./server/src/zipmap.c
            ./server/src/sha1.c
            ./server/src/ziplist.c
            ./server/src/release.c
            ./server/src/networking.c
            ./server/src/util.c
            ./server/src/object.c
            ./server/src/db.c
            ./server/src/replication.c
            ./server/src/rdb.c
            ./server/src/t_string.c
            ./server/src/t_list.c
            ./server/src/t_set.c
            ./server/src/t_zset.c
            ./server/src/t_hash.c
            ./server/src/config.c
            ./server/src/aof.c
            ./server/src/pubsub.c
            ./server/src/multi.c
            ./server/src/debug.c
            ./server/src/sort.c
            ./server/src/intset.c
            ./server/src/syncio.c
            ./server/src/cluster.c
            ./server/src/crc16.c
            ./server/src/endianconv.c
            ./server/src/slowlog.c
            ./server/src/scripting.c
            ./server/src/bio.c
            ./server/src/rio.c
            ./server/src/rand.c
            ./server/src/memtest.c
            ./server/src/crc64.c
            ./server/src/bitops.c
            ./server/src/sentinel.c
            ./server/src/notify.c
            ./server/src/setproctitle.c
            ./server/src/blocked.c
            ./server/src/hyperloglog.c
            ./server/src/latency.c
            ./server/src/sparkline.c
            ./server/src/redis-check-rdb.c
            ./server/src/redis-check-aof.c
            ./server/src/geo.c
            ./server/src/lazyfree.c
            ./server/src/module.c
            ./server/src/evict.c
            ./server/src/expire.c
            ./server/src/geohash.c
            ./server/src/geohash_helper.c
            ./server/src/childinfo.c
            ./server/src/defrag.c
            ./server/src/siphash.c
            ./server/src/rax.c
            ./server/src/t_stream.c
            ./server/src/listpack.c
            ./server/src/server.c
        )
SET(SRC_CLIENT
            ./server/src/ae.c
            ./server/src/anet.c
            ./server/src/adlist.c
            ./server/src/dict.c
            ./server/src/zmalloc.c
            ./server/src/release.c
            ./server/src/crc64.c
            ./server/src/siphash.c
            ./server/src/crc16.c
            ./server/src/redis-cli.c
            ./server/deps/linenoise/linenoise.c
        )

SET(SRC_BENCHMARK
            ./server/src/ae.c
            ./server/src/anet.c
            ./server/src/adlist.c
            ./server/src/zmalloc.c
            ./server/src/redis-benchmark.c
        )
#设置所有需要链接的库名到 LIBRARIES 环境变量
SET(LIBRARIESCLIENT pthread dl rt m libjemalloc.a libhiredis.a)

SET(LIBRARIESSERVER pthread dl rt m libjemalloc.a libhiredis.a liblua.a)

#生成可执行文件
ADD_EXECUTABLE(${PROJECT_NAME}-cli ${SRC_CLIENT})
#把LIBRARIES链接到输出文件中
TARGET_LINK_LIBRARIES(${PROJECT_NAME}-cli ${LIBRARIESCLIENT})

#生成可执行文件
ADD_EXECUTABLE(${PROJECT_NAME}-server ${SRC_SERVER})
#把LIBRARIES链接到输出文件中
TARGET_LINK_LIBRARIES(${PROJECT_NAME}-server ${LIBRARIESSERVER})

###########################################################

