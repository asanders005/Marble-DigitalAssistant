cmake_minimum_required(VERSION 3.13.0)

get_filename_component(ProjectId ${CMAKE_CURRENT_LIST_DIR} NAME)
string(REPLACE " " "_" ProjectId ${ProjectId})

project(${ProjectId} VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
enable_testing()

add_executable(${PROJECT_NAME} 
    
    Source/Audio/audioRecorder.cpp
    Source/Audio/wavHeader.cpp
    Source/Audio/wavFileLoader.cpp
    
    Source/Hardware/Pinboard.cpp

    Source/ImageRec/onnx_classifier.cpp
    Source/ImageRec/onnx_classifier_example.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE asound)
target_link_libraries(${PROJECT_NAME} PRIVATE wiringPi)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_core.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_dnn.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_imgproc.so
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build/lib/libopencv_imgcodecs.so
    )

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/ThirdParty/WiringPi/wiringPi)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Source)
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/core/include
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/dnn/include
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/imgproc/include
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/modules/imgcodecs/include
    ${CMAKE_SOURCE_DIR}/ThirdParty/opencv-4.8.0/build
    )

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)