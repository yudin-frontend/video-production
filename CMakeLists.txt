cmake_minimum_required(VERSION 3.10)
project(video_production CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(video_production src/main.cpp)
target_include_directories(video_production PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(video_production PRIVATE ${OpenCV_LIBS})

# копируем картинки рядом с готовой программой
add_custom_command(TARGET video_production POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:video_production>/assets)
