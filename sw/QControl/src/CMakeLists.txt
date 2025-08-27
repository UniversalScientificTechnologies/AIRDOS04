cmake_minimum_required(VERSION 3.1)

project(QControl)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the QtWidgets library
find_package(Qt6 COMPONENTS Core Quick REQUIRED)

# Instruct CMake to run moc automatically when needed
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Create code from a list of Qt designer ui files
# set(PROJECT_UI 
#    mainwindow.ui
# )

# Populate a CMake variable with the sources
set(PROJECT_SOURCES
    main.cpp
    # mainwindow.cpp
    ${PROJECT_UI}
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# Use the Widgets module from Qt 5
target_link_libraries(${PROJECT_NAME} Qt6::Core Qt6::Quick)
# Specify the C++ include paths
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Specify the directories to install the binaries
install(TARGETS ${PROJECT_NAME} DESTINATION bin)