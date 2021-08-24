# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.10.2)
set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

set(CMAKE_BUILD_TYPE "Debug")
project("x_ffmpeg")

set(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(jni_dir ${CMAKE_CURRENT_SOURCE_DIR}/jni)
set(player_dir ${CMAKE_CURRENT_SOURCE_DIR}/player)
set(tools_dir ${CMAKE_CURRENT_SOURCE_DIR}/tools)

include_directories(${include_dir})
include_directories(${jni_dir}/include)
include_directories(${player_dir}/include)

add_library( # Sets the name of the library.
        ff_player# Sets the library as a shared library.
        SHARED #动态库
        # Provides a relative path to your source file(s).
        ${player_dir}/Player.cpp
        ${player_dir}/PlayerInfo.cpp
        ${player_dir}/Muxer.c
        ${player_dir}/RecorderInfo.cpp
        ${player_dir}/StateListener.cpp
        ${player_dir}/MediaDecoder.cpp
        ${jni_dir}/VMLoader.cpp
       # ${jni_dir}/Looper.cpp
        ${jni_dir}/FFPlayerBridge.cpp
        ${jni_dir}/AVMuxerBridge.cpp
        ${include_dir}/android_log.c
        )

add_library(avcodec
        SHARED
        IMPORTED)
set_target_properties(avcodec
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libavcodec.so)


add_library(avformat
        SHARED
        IMPORTED)

set_target_properties(avformat
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libavformat.so)

add_library(avutil
        SHARED
        IMPORTED)

set_target_properties(avutil
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libavutil.so)



add_library(avfilter
        SHARED
        IMPORTED)
set_target_properties(avfilter
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libavfilter.so)

add_library(swresample
        SHARED
        IMPORTED)
set_target_properties(swresample
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libswresample.so)

add_library(swscale
        SHARED
        IMPORTED)
set_target_properties(swscale
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libswscale.so)


#add_library(ascan
#        SHARED
#        IMPORTED)
#set_target_properties(ascan
#        PROPERTIES IMPORTED_LOCATION
#        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libclang_rt.ubsan_standalone-arm-android.so)


add_library(postproc
        SHARED
        IMPORTED)
set_target_properties(postproc
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libpostproc.so)


find_library(android-log-lib log)


target_link_libraries(ff_player
        android
        mediandk
        #EGL
        #GLESv3
        log
        avcodec
        avfilter
        avformat
        avutil
        swresample
        swscale
        postproc
        )