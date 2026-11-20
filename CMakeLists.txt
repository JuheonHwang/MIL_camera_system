cmake_minimum_required(VERSION 3.16)
project(MIL_camera_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Matrox Imaging Library install directory (the folder containing Include/, LIB/ and DLL/).
set(MIL_ROOT "C:/Program Files/Matrox Imaging/MIL" CACHE PATH "MIL install directory")

# Only tutorials/04_grab_and_save has an OpenCV variant.
option(BUILD_OPENCV_EXAMPLES "Build the examples that need OpenCV" OFF)

# add_mil_example(<target> <source>...)
# Creates one console executable linked against MIL.
function(add_mil_example target)
	add_executable(${target} ${ARGN})
	target_include_directories(${target} PRIVATE
		"${MIL_ROOT}/Include"
		"${PROJECT_SOURCE_DIR}/common")
	target_link_directories(${target} PRIVATE "${MIL_ROOT}/LIB")
	target_link_libraries(${target} PRIVATE Mil)
	target_compile_definitions(${target} PRIVATE UNICODE _UNICODE)
	set_target_properties(${target} PROPERTIES
		VS_DEBUGGER_ENVIRONMENT "PATH=%PATH%;${MIL_ROOT}/DLL")
endfunction()

add_subdirectory(tutorials/01_single_camera_live)
add_subdirectory(tutorials/02_multi_camera_one_pc)
add_subdirectory(tutorials/03_distributed_mil_basics)
add_subdirectory(tutorials/04_grab_and_save)
add_subdirectory(tutorials/05_hardware_trigger)
add_subdirectory(tutorials/06_mdigprocess_multibuffer)
add_subdirectory(tutorials/07_threaded_save_queue)
add_subdirectory(tutorials/08_remote_save_and_move)
add_subdirectory(tutorials/09_timestamp)
add_subdirectory(tutorials/10_color_and_roi)
add_subdirectory(tutorials/11_serial_trigger_generator)
add_subdirectory(apps/multi_camera_capture)
