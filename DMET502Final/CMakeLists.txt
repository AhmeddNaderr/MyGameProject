cmake_minimum_required(VERSION 3.10)
project(GameProject)

set(CMAKE_CXX_STANDARD 11)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

# Include directories
include_directories(${OPENGL_INCLUDE_DIR})
include_directories(${GLUT_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Add executable
add_executable(game main.cpp ObjModel.cpp)

# Link libraries
target_link_libraries(game PRIVATE ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES})
