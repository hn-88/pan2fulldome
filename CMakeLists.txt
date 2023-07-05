cmake_minimum_required(VERSION 3.2)
cmake_policy(SET CMP0037 NEW)
PROJECT(OCVvid2fulldome)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_modules/")

# https://stackoverflow.com/questions/45614230/running-multiple-versions-of-opencv-on-the-same-computer
set(OpenCV_DIR ~/OpenCVLocal/lib/cmake/opencv4 )
find_package(OpenCV REQUIRED)

#include_directories(~/OpenCVLocal/include/opencv4)
include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(OCVvid2fulldome OCVvid2fulldome.cpp tinyfiledialogs.c)
#target_link_libraries(OCVvid2fulldome ~/OpenCVLocal/lib  )
target_link_libraries(OCVvid2fulldome ${OpenCV_LIBS})

