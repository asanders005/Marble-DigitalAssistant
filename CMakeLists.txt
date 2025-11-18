cmake_minimum_required(VERSION 3.13.0)

get_filename_component(ProjectId ${CMAKE_CURRENT_LIST_DIR} NAME)
string(REPLACE " " "_" ProjectId ${ProjectId})

project(${ProjectId} VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find libcamera via pkg-config so we get proper include/link flags
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED libcamera)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
find_package(Boost REQUIRED COMPONENTS program_options)
find_package(yaml-cpp REQUIRED)

include(CTest)
enable_testing()

add_executable(${PROJECT_NAME} 
    Source/main.cpp
    
    Source/Audio/audioRecorder.cpp
    Source/Audio/wavHeader.cpp
    Source/Audio/wavFileLoader.cpp
    
    Source/Hardware/Pinboard.cpp

    Source/Camera/GStreamer.cpp
    Source/Camera/GstRecorder.cpp
    
    Source/ImageRec/YoloModel.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Source)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/ThirdParty/nlohmannJson)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/ThirdParty/WiringPi/wiringPi)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/include)
target_include_directories(${PROJECT_NAME} PRIVATE 
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/core/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/dnn/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/imgproc/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/imgcodecs/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/calib3d/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/highgui/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/videoio/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/features2d/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/flann/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/ml/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/objdetect/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/photo/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/stitching/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/video/include
${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/videoio/include
)
target_include_directories(${PROJECT_NAME} PRIVATE /usr/include/rpicam-apps)
target_include_directories(${PROJECT_NAME} PRIVATE 
${GSTREAMER_INCLUDE_DIRS}
${GSTREAMER_APP_INCLUDE_DIRS}
${GSTREAMER_VIDEO_INCLUDE_DIRS}
${GLIB2_INCLUDE_DIRS}
)
target_include_directories(${PROJECT_NAME} PRIVATE ${LIBCAMERA_INCLUDE_DIRS})

target_compile_options(${PROJECT_NAME} PRIVATE ${LIBCAMERA_CFLAGS_OTHER})
target_compile_options(${PROJECT_NAME} PRIVATE 
${GSTREAMER_CFLAGS_OTHER}
${GSTREAMER_APP_CFLAGS_OTHER}
${GSTREAMER_VIDEO_CFLAGS_OTHER}
${GLIB2_CFLAGS_OTHER}
)

target_link_libraries(${PROJECT_NAME} PRIVATE asound)
target_link_libraries(${PROJECT_NAME} PRIVATE wiringPi)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBCAMERA_LIBRARIES} -lrpicam_app)
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    ${GLIB2_LIBRARIES}
    )
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::program_options)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_core.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_dnn.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_imgproc.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_imgcodecs.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_videoio.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_highgui.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_features2d.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_flann.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_ml.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_objdetect.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_photo.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_stitching.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_video.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_videoio.so
    )
target_link_libraries(${PROJECT_NAME} PRIVATE yaml-cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)