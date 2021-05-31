# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.10.2)
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
      #  ${player_dir}/AsyncQueue.cpp
        ${player_dir}/PlayerInfo.cpp
        ${player_dir}/Muxer.c
        ${player_dir}/RecorderInfo.cpp
        ${player_dir}/StateListener.cpp
        ${jni_dir}/FFPlayerBridge.cpp
        )

add_library(avcodec-57
        SHARED
        IMPORTED)
set_target_properties(avcodec-57
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libavcodec-57.so)


add_library(avformat-57
        SHARED
        IMPORTED)
set_target_properties(avformat-57
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libavformat-57.so)

add_library(avutil-55
        SHARED
        IMPORTED)
set_target_properties(avutil-55
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libavutil-55.so)




add_library(avdevice-57
        SHARED
        IMPORTED)
set_target_properties(avdevice-57
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libavdevice-57.so)

add_library(avfilter-6
        SHARED
        IMPORTED)
set_target_properties(avfilter-6
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libavfilter-6.so)

add_library(swresample-2
        SHARED
        IMPORTED)
set_target_properties(swresample-2
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libswresample-2.so)

add_library(swscale-4
        SHARED
        IMPORTED)
set_target_properties(swscale-4
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libswscale-4.so)

add_library(postproc-54
        SHARED
        IMPORTED)
set_target_properties(postproc-54
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libpostproc-54.so)

find_library(android-log-lib log)


target_link_libraries(ff_player
        android
        mediandk
        #EGL
        #GLESv3
        log

        avcodec-57
        avdevice-57
        avfilter-6
        avformat-57
        avutil-55
        swresample-2
        swscale-4
        postproc-54
        )