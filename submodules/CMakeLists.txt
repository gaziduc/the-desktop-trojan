set(SDL_TEST OFF)

# Use vendored libs
set(SDLTTF_VENDORED ON)

add_subdirectory(SDL)
add_subdirectory(SDL_ttf)

add_library(submodules INTERFACE)
target_link_libraries(submodules INTERFACE SDL3::SDL3 SDL3_ttf::SDL3_ttf)

