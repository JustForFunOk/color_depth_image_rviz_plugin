cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(color_depth_image_display)

add_compile_options(
  -std=c++11
  -pthread
  -Wall
  -Wextra
  -Werror)

# error: unused parameter [-Werror=unused-parameter]
add_compile_options(-Wno-unused-parameter -Wno-unused-but-set-parameter)

find_package(catkin REQUIRED COMPONENTS roscpp rviz)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

include_directories(include ${catkin_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS}
                    ${Qt5Core_INCLUDE_DIRS})

catkin_package(
  INCLUDE_DIRS
  include
  LIBRARIES
  ${PROJECT_NAME}
  CATKIN_DEPENDS
  roscpp
  rviz
  DEPENDS)

qt5_wrap_cpp(${PROJECT_NAME}_MOCS include/color_depth_image_display.h)

add_library(${PROJECT_NAME} src/color_depth_image_display.cpp src/colorizer.cpp ${${PROJECT_NAME}_MOCS})

target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES} Qt5::Core Qt5::Gui)
