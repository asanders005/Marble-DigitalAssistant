cmake_minimum_required(VERSION 3.13.0)

get_filename_component(ProjectId ${CMAKE_CURRENT_LIST_DIR} NAME)
string(REPLACE " " "_" ProjectId ${ProjectId})

project(${ProjectId} VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(${PROJECT_NAME} 
    Source/main.cpp
    Source/Audio/audioRecorder.cpp
    Source/Audio/wavHeader.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE asound)
target_link_libraries(${PROJECT_NAME} PRIVATE wiringPi)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/WiringPi/wiringPi)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Source)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)