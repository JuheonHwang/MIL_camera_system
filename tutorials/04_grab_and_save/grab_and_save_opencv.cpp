// 04_grab_and_save_opencv.cpp
//
// What this step adds over chapter 03:
//   Chapters 01-03 only displayed images. Here every camera is grabbed with a
//   plain MdigGrab (no trigger, the camera free-runs) and the frames are
//   written to disk. Saving is done with OpenCV: the pixels are copied out of
//   the MIL buffer into a cv::Mat with MbufGet, then cv::imwrite writes a PNG.
//   This is the only example in the repo that depends on OpenCV; later
//   chapters save with MIL's own functions.
//
// Also new: this chapter is the first to use common/mil_common.h. The
// milcam:: helpers replace the allocation, camera-settings and buffer code
// that chapters 01-03 wrote out by hand.
//
// Key MIL functions (new here): MdigGrab, MbufGet.
//
// Cameras: two boards on this PC and two boards on a second PC that runs the
// Distributed MIL server (same layout as chapter 03).
//
// At run time: nothing to press. The program grabs NUM_FRAMES frames from each
// camera in turn, saves them to OUTPUT_DIR as <frame>_<camera>.png and exits.
//
// Build: needs OpenCV, configure with -DBUILD_OPENCV_EXAMPLES=ON.

#include <mil.h>
#include "mil_common.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// Address of the second PC (runs the Distributed MIL server). Replace with yours.
static const MIL_STRING REMOTE_PC = MIL_TEXT("192.168.0.2");

// system descriptor, board number, digitizer number (as in the original program).
static const std::vector<milcam::CameraSource> CAMERAS = {
	{ M_SYSTEM_RAPIXOCXP,                 M_DEV0, M_DEV3 },
	{ M_SYSTEM_RAPIXOCXP,                 M_DEV1, M_DEV3 },
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV0, M_DEV3 },
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV1, M_DEV3 },
};

static const int NUM_FRAMES = 5;

// cv::imwrite takes a narrow std::string path, so this one name is a plain
// std::string and is built directly, not converted from MIL_STRING.
static const std::string OUTPUT_DIR = "C:/output_image/";

// 60 ms exposure -> at most 16.6 frames per second (the original computed
// floor(1000000 / ExposureTime * 10) / 10).
static milcam::CameraSettings MakeSettings()
{
	milcam::CameraSettings Settings;
	Settings.ExposureTime = 60000;
	Settings.AcquisitionFrameRate = 16.6;
	return Settings;
}

int MosMain(void)
{
	const milcam::CameraSettings Settings = MakeSettings();
	const size_t NumCameras = CAMERAS.size();

	MIL_ID MilApplication = M_NULL;
	std::vector<MIL_ID> MilSystem(NumCameras, M_NULL);
	std::vector<MIL_ID> MilDigitizer(NumCameras, M_NULL);
	std::vector<MIL_ID> MilImage(NumCameras, M_NULL);

	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	for (size_t i = 0; i < NumCameras; i++)
	{
		MilSystem[i] = milcam::AllocSystem(CAMERAS[i]);
		MilDigitizer[i] = milcam::AllocDigitizer(MilSystem[i], CAMERAS[i]);
	}

	for (size_t i = 0; i < NumCameras; i++)
		milcam::ApplyCameraSettings(MilDigitizer[i], Settings);

	// The buffer lives on the same system as its digitizer, so for the two
	// remote cameras it is allocated on the remote PC.
	for (size_t i = 0; i < NumCameras; i++)
		MilImage[i] = milcam::AllocMono8Buffer(MilSystem[i], MilDigitizer[i], M_IMAGE + M_DISP + M_GRAB);

	// Make sure the cameras free-run: with TriggerMode Off, MdigGrab starts an
	// acquisition on its own instead of waiting for an external pulse.
	for (size_t i = 0; i < NumCameras; i++)
	{
		MdigControlFeature(MilDigitizer[i], M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
		MdigControlFeature(MilDigitizer[i], M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("Off"));
	}

	std::filesystem::create_directories(OUTPUT_DIR);

	// One cv::Mat per camera, reused for every frame. CV_8U matches the Mono8
	// MIL buffer: one byte per pixel, rows packed with no padding.
	std::vector<cv::Mat> CvImage(NumCameras);
	for (size_t i = 0; i < NumCameras; i++)
		CvImage[i].create(cv::Size((int)Settings.Width, (int)Settings.Height), CV_8U);

	for (int Frame = 0; Frame < NUM_FRAMES; Frame++)
	{
		// MdigGrab blocks until one frame has arrived in the buffer.
		for (size_t i = 0; i < NumCameras; i++)
			MdigGrab(MilDigitizer[i], MilImage[i]);

		// MbufGet copies the pixels of the MIL buffer into the memory we pass.
		// That memory is the cv::Mat's own data block. Because it is a copy,
		// it also works for the buffers that sit on the remote PC.
		for (size_t i = 0; i < NumCameras; i++)
			MbufGet(MilImage[i], CvImage[i].data);

		for (size_t i = 0; i < NumCameras; i++)
		{
			std::ostringstream FileName;
			FileName << OUTPUT_DIR
				<< std::setw(4) << std::setfill('0') << Frame
				<< "_"
				<< std::setw(2) << std::setfill('0') << (int)i
				<< ".png";
			if (!cv::imwrite(FileName.str(), CvImage[i]))
				MosPrintf(MIL_TEXT("Failed to write frame %d of camera %d.\n"), Frame, (int)i);
		}

		MosPrintf(MIL_TEXT("Frame %d saved.\n"), Frame);
	}

	MosPrintf(MIL_TEXT("Image Saved!\n"));

	for (size_t i = 0; i < NumCameras; i++)
	{
		MbufFree(MilImage[i]);
		MdigFree(MilDigitizer[i]);
		MsysFree(MilSystem[i]);
	}
	MappFree(MilApplication);

	return 0;
}
