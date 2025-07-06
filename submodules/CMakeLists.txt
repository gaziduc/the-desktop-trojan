set(SDL_TEST OFF)

# Use vendored libs
set(SDLTTF_VENDORED ON)
set(BUILD_SHARED_LIBS ON)

add_subdirectory(SDL EXCLUDE_FROM_ALL)
add_subdirectory(SDL_ttf EXCLUDE_FROM_ALL)

add_library(submodules INTERFACE)
target_link_libraries(submodules INTERFACE SDL3::SDL3 SDL3_ttf::SDL3_ttf)

