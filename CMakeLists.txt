cmake_minimum_required(VERSION 3.22)
project(VST3Test VERSION 0.2.0 LANGUAGES C CXX)

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

add_subdirectory(SimpleSynth)
add_subdirectory(Murmur)
