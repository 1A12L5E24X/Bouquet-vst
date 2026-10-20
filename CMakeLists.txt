cmake_minimum_required(VERSION 3.22)

# Mac first: run on macOS 11+ (universal arm64 + x86_64 is chosen in CI).
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
# The version shown to hosts and in the installer comes from the VERSION file.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" BOUQUET_VERSION)
string(STRIP "${BOUQUET_VERSION}" BOUQUET_VERSION)
project(VST3Test VERSION ${BOUQUET_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

option(VST3TEST_BUILD_TESTS "Build the offline render tests" ON)
if(VST3TEST_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(Bouquet)
