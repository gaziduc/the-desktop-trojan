add_executable(${PROJECT_NAME})
add_compile_options(-Wall -Wextra)
target_sources(${PROJECT_NAME} PRIVATE main.cpp framerate.cpp framerate.h sdlwrapper.cpp sdlwrapper.h resources.cpp resources.h)
target_link_libraries(${PROJECT_NAME} PRIVATE submodules)