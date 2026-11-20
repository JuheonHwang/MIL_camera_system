// roi_capture.cpp
//
// Chapter 10b: capture a region of interest (ROI) cut out on the sensor.
//
// What this adds over chapters 06/07 (MdigProcess multi-buffer + threaded save):
//   Marked "NEW (10b)":
//   1. Width/Height are reduced (5120 -> 2560) and OffsetX/OffsetY (1280, 1280) are
//      written to the camera, each followed by the matching frame grabber control
//      (M_SOURCE_OFFSET_X / M_SOURCE_OFFSET_Y). The camera then sends only that
//      window of the sensor, so every frame is smaller (less data on the link,
//      in memory and on disk).
//   2. Buffers are allocated with the ROI size (2560 x 2560), not the full sensor.
//
// Order of the settings (kept exactly as in the original; on GenICam cameras the
// order can matter because OffsetX + Width must fit inside the sensor):
//   Height, M_SOURCE_SIZE_Y, Width, M_SOURCE_SIZE_X, OffsetX, M_SOURCE_OFFSET_X,
//   OffsetY, M_SOURCE_OFFSET_Y, PixelFormat, ExposureTime, AcquisitionFrameRate.
//
// Key MIL functions: MdigControlFeature ("OffsetX", "OffsetY"), MdigControl
//   (M_SOURCE_OFFSET_X / _Y), MbufAlloc2d, MdigProcess, MbufClone, MbufSave.
//
// Run time: press a key to start (the first grab waits for the external trigger),
//   press a key to stop, press a key to exit. Files: <frame>_<camera>.tiff in
//   C:/output_image/ (images from the remote PC are saved on that PC and are
//   not moved here; see chapter 08 for the move step).

#include <mil.h>
#include "mil_common.h"

#include <thread>
#include <vector>

// Address of the second PC (runs the Distributed MIL server). Replace with yours.
static const MIL_STRING REMOTE_PC = MIL_TEXT("192.168.0.2");

// Note: every digitizer here is M_DEV3, as in the original.
static const std::vector<milcam::CameraSource> CAMERAS = {
	{ M_SYSTEM_RAPIXOCXP,                 M_DEV0, M_DEV3 },
	{ M_SYSTEM_RAPIXOCXP,                 M_DEV1, M_DEV3 },
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV0, M_DEV3 },
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV1, M_DEV3 },
};

static const int N_FRAME = 50;
static const int SAVE_THREADS = 12;
static const MIL_STRING OUTPUT_DIR = MIL_TEXT("C:/output_image/");

// NEW (10b): the region of interest. Offset + size must stay inside 5120 x 5120.
static const MIL_INT64 ROI_WIDTH = 2560;
static const MIL_INT64 ROI_HEIGHT = 2560;
static const MIL_INT64 ROI_OFFSET_X = 1280;
static const MIL_INT64 ROI_OFFSET_Y = 1280;

static const MIL_DOUBLE EXPOSURE_TIME = 60000;
static const MIL_DOUBLE FRAME_RATE = 15;

struct SaveItem
{
	MIL_ID Image;
	bool Remote;
	int FrameIndex;
	int CameraIndex;
};

struct HookData
{
	milcam::WorkQueue<SaveItem>* SaveQueue;
	bool Remote;
	int CameraIndex;
	MIL_INT ProcessedImageCount;
};

MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr)
{
	HookData* Hook = (HookData*)HookDataPtr;
	MIL_ID GrabbedBuffer;
	MIL_ID CloneId;

	Hook->ProcessedImageCount++;
	MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &GrabbedBuffer);
	MbufClone(GrabbedBuffer, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_IMAGE + M_HOST_MEMORY + M_OFF_BOARD, M_COPY_SOURCE_DATA, &CloneId);

	Hook->SaveQueue->Push({ CloneId, Hook->Remote, (int)Hook->ProcessedImageCount, Hook->CameraIndex });
	return 0;
}

static void SaveWorker(milcam::WorkQueue<SaveItem>& SaveQueue)
{
	SaveItem Item;
	while (SaveQueue.Pop(Item))
	{
		MIL_STRING FileName = milcam::FrameFileName(OUTPUT_DIR, Item.FrameIndex, Item.CameraIndex);
		if (Item.Remote)
			FileName = MIL_TEXT("remote:///") + FileName;   // write on the PC that owns the buffer
		MbufSave(FileName.c_str(), Item.Image);
		MbufFree(Item.Image);
	}
}

int MosMain(void)
{
	const int N_CAM = (int)CAMERAS.size();

	MIL_ID MilApplication = M_NULL;
	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	std::vector<MIL_ID> MilSystem(N_CAM);
	std::vector<MIL_ID> MilDigitizer(N_CAM);
	std::vector<std::vector<MIL_ID> > GrabBuffers(N_CAM, std::vector<MIL_ID>(N_FRAME));
	std::vector<HookData> Hooks(N_CAM);
	milcam::WorkQueue<SaveItem> SaveQueue;

	for (int n = 0; n < N_CAM; n++)
	{
		MilSystem[n] = milcam::AllocSystem(CAMERAS[n]);
		MsysControl(MilSystem[n], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
		MilDigitizer[n] = milcam::AllocDigitizer(MilSystem[n], CAMERAS[n]);
	}

	for (int n = 0; n < N_CAM; n++)
	{
		MIL_ID Dig = MilDigitizer[n];

		MIL_INT64 Height = ROI_HEIGHT;
		MdigControlFeature(Dig, M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
		MdigControl(Dig, M_SOURCE_SIZE_Y, Height);

		MIL_INT64 Width = ROI_WIDTH;
		MdigControlFeature(Dig, M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
		MdigControl(Dig, M_SOURCE_SIZE_X, Width);

		// NEW (10b): the window position. Written after Width/Height, camera first,
		// then the grabber's matching source offset.
		MIL_INT64 OffsetX = ROI_OFFSET_X;
		MdigControlFeature(Dig, M_FEATURE_VALUE, MIL_TEXT("OffsetX"), M_TYPE_INT64, &OffsetX);
		MdigControl(Dig, M_SOURCE_OFFSET_X, OffsetX);

		MIL_INT64 OffsetY = ROI_OFFSET_Y;
		MdigControlFeature(Dig, M_FEATURE_VALUE, MIL_TEXT("OffsetY"), M_TYPE_INT64, &OffsetY);
		MdigControl(Dig, M_SOURCE_OFFSET_Y, OffsetY);

		MdigControlFeature(Dig, M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));

		MIL_DOUBLE ExposureTime = EXPOSURE_TIME;
		MdigControlFeature(Dig, M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);

		MIL_DOUBLE AcquisitionFrameRate = FRAME_RATE;
		MdigControlFeature(Dig, M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);

		milcam::EnableExternalTrigger(Dig);
	}

	// NEW (10b): buffers sized to the ROI. AllocMono8Buffer would also work here
	// (it asks the digitizer for its size); the original used the constants.
	for (int n = 0; n < N_CAM; n++)
	{
		for (int f = 0; f < N_FRAME; f++)
		{
			MbufAlloc2d(MilSystem[n], (MIL_INT)ROI_WIDTH, (MIL_INT)ROI_HEIGHT, 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &GrabBuffers[n][f]);
			milcam::RequireAllocated(GrabBuffers[n][f], MIL_TEXT("ROI grab buffer"));
			MbufClear(GrabBuffers[n][f], 0L);
		}
	}

	for (int n = 0; n < N_CAM; n++)
	{
		Hooks[n].SaveQueue = &SaveQueue;
		Hooks[n].Remote = milcam::IsRemoteSystem(MilSystem[n]);
		Hooks[n].CameraIndex = n + 1;
		Hooks[n].ProcessedImageCount = 0;
		MdigControl(MilDigitizer[n], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
	}

	MosPrintf(MIL_TEXT("Press Any Key to Start Process...\n"));
	MosGetch();

	for (int n = 0; n < N_CAM; n++)
		MdigProcess(MilDigitizer[n], GrabBuffers[n].data(), N_FRAME, M_START, M_TRIGGER_FOR_FIRST_GRAB, ProcessingFunction, &Hooks[n]);

	std::vector<std::thread> SaveThreads;
	for (int n = 0; n < SAVE_THREADS; n++)
		SaveThreads.emplace_back(SaveWorker, std::ref(SaveQueue));

	MosPrintf(MIL_TEXT("Press Any Key to Stop Process...\n"));
	MosGetch();

	// Stop the producers, close the queue, then join the workers.
	for (int n = 0; n < N_CAM; n++)
		MdigProcess(MilDigitizer[n], GrabBuffers[n].data(), N_FRAME, M_STOP, M_DEFAULT, ProcessingFunction, &Hooks[n]);
	SaveQueue.Close();
	for (std::thread& Thread : SaveThreads)
		Thread.join();

	for (int n = 0; n < N_CAM; n++)
	{
		MIL_INT ProcessFrameCount = 0;
		MIL_INT ProcessFrameMissedCount = 0;
		MIL_DOUBLE ProcessFrameRate = 0;
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_COUNT, &ProcessFrameCount);
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_MISSED, &ProcessFrameMissedCount);
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_RATE, &ProcessFrameRate);
		MosPrintf(MIL_TEXT("Digitizer %d: %d frames grabbed at %.1f frames/sec, %d frames missed\n"),
			n + 1, (int)ProcessFrameCount, ProcessFrameRate, (int)ProcessFrameMissedCount);
	}

	MosPrintf(MIL_TEXT("Process Done!\n"));
	MosGetch();

	for (int n = 0; n < N_CAM; n++)
		for (int f = 0; f < N_FRAME; f++)
			MbufFree(GrabBuffers[n][f]);
	for (int n = 0; n < N_CAM; n++)
	{
		MdigFree(MilDigitizer[n]);
		MsysFree(MilSystem[n]);
	}
	MappFree(MilApplication);
	return 0;
}
