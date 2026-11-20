# This chapter has a single example and it needs OpenCV, so it is only built
# when configured with -DBUILD_OPENCV_EXAMPLES=ON.
if(BUILD_OPENCV_EXAMPLES)
	find_package(OpenCV REQUIRED)
	add_mil_example(04_grab_and_save_opencv grab_and_save_opencv.cpp)
	target_include_directories(04_grab_and_save_opencv PRIVATE ${OpenCV_INCLUDE_DIRS})
	target_link_libraries(04_grab_and_save_opencv PRIVATE ${OpenCV_LIBS})
endif()
