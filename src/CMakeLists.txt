add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE main.cpp framerate.cpp framerate.h)
target_link_libraries(${PROJECT_NAME} PRIVATE submodules)