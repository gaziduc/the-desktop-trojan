# Disable tests
set(SDL_TEST OFF)
set(SDLIMAGE_TESTS OFF)

# Use vendored libs
set(SDLTTF_VENDORED ON)
set(SDLMIXER_VENDORED ON)
set(SDLIMAGE_VENDORED ON)

# Disable not used features
# SDL_image formats
set(SDLIMAGE_AVIF OFF)
set(SDLIMAGE_GIF OFF)
set(SDLIMAGE_JPG OFF)
set(SDLIMAGE_JXL OFF)
set(SDLIMAGE_LBM OFF)
set(SDLIMAGE_PCX OFF)
set(SDLIMAGE_PNM OFF)
set(SDLIMAGE_QOI OFF)
set(SDLIMAGE_SVG OFF)
set(SDLIMAGE_TGA OFF)
set(SDLIMAGE_TIF OFF)
set(SDLIMAGE_WEBP OFF)
set(SDLIMAGE_XCF OFF)
set(SDLIMAGE_XPM OFF)
set(SDLIMAGE_XV OFF)
# SDL_ttf features
set(SDLTTF_HARFBUZZ OFF)
set(SDLTTF_PLUTOSVG OFF)
# SDL_image formats
set(SDLMIXER_FLAC OFF)
set(SDLMIXER_GME OFF)
set(SDLMIXER_MOD OFF)
set(SDLMIXER_MIDI OFF)
set(SDLMIXER_OPUS OFF)

add_subdirectory(SDL EXCLUDE_FROM_ALL)
add_subdirectory(SDL_ttf EXCLUDE_FROM_ALL)
add_subdirectory(SDL_mixer EXCLUDE_FROM_ALL)
add_subdirectory(SDL_image EXCLUDE_FROM_ALL)
set(ZLIB_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/zlib)
set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/zlib)
add_subdirectory(libzip EXCLUDE_FROM_ALL)

add_library(submodules INTERFACE)
target_link_libraries(submodules INTERFACE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_mixer::SDL3_mixer SDL3_image::SDL3_image zip)

