// color_capture.cpp
//
// Chapter 10a: one colour (Bayer) camera, using the same pipeline as the mono examples.
//
// What this adds over chapters 07/08 (threaded save queue):
//   Two things change, both marked "NEW (10a)":
//   1. The camera PixelFormat feature is "BayerGB8" instead of "Mono8".
//   2. Image buffers are allocated with MbufAllocColor, taking the number of bands
//      from the digitizer (MdigInquire M_SIZE_BAND), instead of MbufAlloc2d with
//      8 + M_UNSIGNED. The helper milcam::AllocMono8Buffer is mono-only, so the
//      allocation is written out here.
//   Everything else (hook, MbufClone, queue, save thread) is unchanged. The original
//   calls no MIL colour-conversion function: the saved TIFF holds whatever band
//   layout the digitizer reports.
//
// Key MIL functions: MdigControlFeature (PixelFormat), MdigInquire (M_SIZE_BAND),
//   MbufAllocColor, MdigProcess, MbufClone, MbufSave.
//
// Run time: press a key to start grabbing (the first grab waits for the external
//   trigger), press a key again to stop, then a third time to exit.
//   Files: C:/output_image/<frame>_01.tiff

#include <mil.h>
#include "mil_common.h"

#include <thread>
#include <vector>

// One local camera: system M_DEV2 of the Rapixo CXP board, digitizer M_DEV0.
static const milcam::CameraSource CAMERA = { M_SYSTEM_RAPIXOCXP, M_DEV2, M_DEV0 };

static const int N_FRAME = 45;
static const int SAVE_THREADS = 4;
static const int CAMERA_INDEX = 1;
static const MIL_STRING OUTPUT_DIR = MIL_TEXT("C:/output_image/");

static const MIL_INT64 IMAGE_WIDTH = 5120;
static const MIL_INT64 IMAGE_HEIGHT = 5120;
static const MIL_DOUBLE EXPOSURE_TIME = 30000;
static const MIL_DOUBLE FRAME_RATE = 30;

struct SaveItem
{
	MIL_ID Image;
	int FrameIndex;
};

struct HookData
{
	milcam::WorkQueue<SaveItem>* SaveQueue;
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

	Hook->SaveQueue->Push({ CloneId, (int)Hook->ProcessedImageCount });
	return 0;
}

static void SaveWorker(milcam::WorkQueue<SaveItem>& SaveQueue)
{
	SaveItem Item;
	while (SaveQueue.Pop(Item))
	{
		MIL_STRING FileName = milcam::FrameFileName(OUTPUT_DIR, Item.FrameIndex, CAMERA_INDEX);
		MbufSave(FileName.c_str(), Item.Image);
		MbufFree(Item.Image);
	}
}

int MosMain(void)
{
	MIL_ID MilApplication = M_NULL;
	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	MIL_ID MilSystem = milcam::AllocSystem(CAMERA);
	MsysControl(MilSystem, M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MIL_ID MilDigitizer = milcam::AllocDigitizer(MilSystem, CAMERA);

	// Camera settings, in the original order. Written out (not ApplyCameraSettings)
	// because the original sets no Gain, and the pixel format differs.
	MIL_INT64 Height = IMAGE_HEIGHT;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControl(MilDigitizer, M_SOURCE_SIZE_Y, Height);

	MIL_INT64 Width = IMAGE_WIDTH;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControl(MilDigitizer, M_SOURCE_SIZE_X, Width);

	// NEW (10a): the sensor delivers a Bayer mosaic, not grey values.
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("BayerGB8"));

	MIL_DOUBLE ExposureTime = EXPOSURE_TIME;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);

	MIL_DOUBLE AcquisitionFrameRate = FRAME_RATE;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);

	// NEW (10a): colour buffers. The band count comes from the digitizer, so the
	// buffer matches whatever PixelFormat was just selected. Allocated after the
	// PixelFormat is written, as in the original.
	MIL_ID GrabBuffers[N_FRAME];
	for (int n = 0; n < N_FRAME; n++)
	{
		MbufAllocColor(MilSystem,
			(MIL_INT)(MdigInquire(MilDigitizer, M_SIZE_BAND, M_NULL)),
			(MIL_INT)(MdigInquire(MilDigitizer, M_SIZE_X, M_NULL)),
			(MIL_INT)(MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL)),
			8 + M_UNSIGNED, M_IMAGE + M_GRAB, &GrabBuffers[n]);
		milcam::RequireAllocated(GrabBuffers[n], MIL_TEXT("colour grab buffer"));
		MbufClear(GrabBuffers[n], 0L);
	}

	milcam::EnableExternalTrigger(MilDigitizer);

	milcam::WorkQueue<SaveItem> SaveQueue;
	HookData Hook = { &SaveQueue, 0 };

	MdigControl(MilDigitizer, M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);

	MosPrintf(MIL_TEXT("Press Any Key to Start Process...\n"));
	MosGetch();

	MdigProcess(MilDigitizer, GrabBuffers, N_FRAME, M_START, M_TRIGGER_FOR_FIRST_GRAB, ProcessingFunction, &Hook);

	std::vector<std::thread> SaveThreads;
	for (int n = 0; n < SAVE_THREADS; n++)
		SaveThreads.emplace_back(SaveWorker, std::ref(SaveQueue));

	MosPrintf(MIL_TEXT("Press Any Key to Stop Process...\n"));
	MosGetch();

	// Stop the producer, close the queue, then join the workers.
	MdigProcess(MilDigitizer, GrabBuffers, N_FRAME, M_STOP, M_DEFAULT, ProcessingFunction, &Hook);
	SaveQueue.Close();
	for (std::thread& Thread : SaveThreads)
		Thread.join();

	MIL_INT ProcessFrameCount = 0;
	MIL_INT ProcessFrameMissedCount = 0;
	MIL_DOUBLE ProcessFrameRate = 0;
	MdigInquire(MilDigitizer, M_PROCESS_FRAME_COUNT, &ProcessFrameCount);
	MdigInquire(MilDigitizer, M_PROCESS_FRAME_MISSED, &ProcessFrameMissedCount);
	MdigInquire(MilDigitizer, M_PROCESS_FRAME_RATE, &ProcessFrameRate);
	MosPrintf(MIL_TEXT("First Digitizer: %d frames grabbed at %.1f frames/sec, %d frames missed\n"),
		(int)ProcessFrameCount, ProcessFrameRate, (int)ProcessFrameMissedCount);

	MosPrintf(MIL_TEXT("Process Done!\n"));
	MosGetch();

	for (int n = 0; n < N_FRAME; n++)
		MbufFree(GrabBuffers[n]);
	MdigFree(MilDigitizer);
	MsysFree(MilSystem);
	MappFree(MilApplication);
	return 0;
}
