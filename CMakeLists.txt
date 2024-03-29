cmake_minimum_required(VERSION 3.16)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")
project(SampleCreator VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(libs/sst-rackhelpers)
add_subdirectory(libs/sst-cpputils)

set(PLUGIN_NAME ${PROJECT_NAME})
set(ADDITIONAL_PLUGIN_DISTRIBUTABLES res LICENSE.md README.md)
include(RackSDK.cmake)

message(STATUS "SampleCreator for Rack Build Process" )
message(STATUS "Installing into '${CMAKE_INSTALL_PREFIX}'")

add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden)

target_include_directories(${RACK_PLUGIN_LIB} PRIVATE src src/autogen_airwin/)
target_sources(${RACK_PLUGIN_LIB} PRIVATE
        src/SampleCreator.cpp
        src/SampleCreatorModule.cpp
        )

target_compile_options(${RACK_PLUGIN_LIB} PUBLIC -Wno-suggest-override -Wno-multichar -Wno-unused-value -Wno-unused-but-set-variable -Wno-unused-variable )
target_link_libraries(${RACK_PLUGIN_LIB} PUBLIC sst-rackhelpers sst-cpputils)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(${RACK_PLUGIN_LIB} PUBLIC -Wno-stringop-truncation)
endif()
