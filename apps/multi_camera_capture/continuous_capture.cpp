// continuous_capture.cpp
//
// Finished application, variant 1: continuous hardware-triggered capture on 10
// cameras (5 PCs, 2 Rapixo CXP boards each), saved on each PC, then gathered
// on the host.
//
// Builds on tutorials 05-08: hardware trigger + MdigProcess multi-buffer +
// threaded save queue + remote save and move. Shared parts live in
// capture_common.h; this file holds the capture loop and the shutdown order.
//
// Key MIL functions: MdigProcess (M_START / M_STOP), MdigGetHookInfo,
// MbufClone, MbufSave, MappFileOperation, MdigInquire (M_PROCESS_FRAME_*).
//
// At run time:
//   1. Press any key to start MdigProcess on all cameras. Frames are grabbed
//      on each trigger pulse and saved as C:/output_image/FFFF_CC.tiff
//      (FFFF = frame, CC = camera 01..10).
//   2. Press any key to stop. The program finishes saving, moves the files of
//      the remote PCs to the host, prints statistics, then waits for a key.

#include <mil.h>

#include "capture_common.h"

// Camera features written to every camera.
static milcam::CameraSettings MakeSettings()
{
	milcam::CameraSettings Settings;     // 5120 x 5120, Mono8, gain 1 by default
	Settings.ExposureTime = 60000;       // microseconds
	Settings.AcquisitionFrameRate = 15;  // frames per second
	return Settings;
}

static const int GRAB_BUFFERS = 60;  // buffers per camera handed to MdigProcess
static const int SAVE_THREADS = 8;
static const int MOVE_THREADS = 8;

// Called by MdigProcess for every grabbed frame (on a MIL thread, possibly
// several at once because of M_MULTI_THREAD). Must return quickly: it only
// clones the buffer and queues it.
static MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr)
{
	capture::CameraHookData* Camera = (capture::CameraHookData*)HookDataPtr;
	int FrameIndex = (int)(++Camera->FrameCount);  // 1-based

	MIL_ID ModifiedBuffer = M_NULL;
	MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &ModifiedBuffer);

	capture::QueueFrameForSave(*Camera, ModifiedBuffer, FrameIndex);
	return 0;
}

int MosMain(void)
{
	MIL_ID MilApplication = M_NULL;
	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);
	MappControlMp(M_DEFAULT, M_MP_USE, M_DEFAULT, M_ENABLE, M_NULL);

	std::vector<capture::CameraRig> Rigs(capture::CAMERAS.size());
	capture::SaveQueue SaveQueue;
	capture::MoveQueue MoveQueue;

	capture::AllocSystemsAndDigitizers(Rigs);
	capture::ConfigureCameras(Rigs, MakeSettings());
	capture::AllocGrabBuffers(Rigs, GRAB_BUFFERS);
	capture::InitHookData(Rigs, SaveQueue);
	capture::ResetMissedFrameCounters(Rigs);

	MosPrintf(MIL_TEXT("Press Any Key to Start Process...\n"));
	MosGetch();

	// M_TRIGGER_FOR_FIRST_GRAB: the first frame waits for the first trigger pulse.
	for (capture::CameraRig& Rig : Rigs)
		MdigProcess(Rig.Digitizer, Rig.Buffers.data(), GRAB_BUFFERS, M_START, M_TRIGGER_FOR_FIRST_GRAB, ProcessingFunction, &Rig.Hook);

	std::vector<std::thread> SaveWorkers;
	for (int n = 0; n < SAVE_THREADS; n++)
		SaveWorkers.emplace_back(capture::SaveWorker, std::ref(SaveQueue), std::ref(MoveQueue));

	MosPrintf(MIL_TEXT("Press Any Key to Stop Process...\n"));
	MosGetch();

	// Shutdown order matters:
	// 1. stop grabbing, so nothing new is pushed to the save queue;
	for (capture::CameraRig& Rig : Rigs)
		MdigProcess(Rig.Digitizer, Rig.Buffers.data(), GRAB_BUFFERS, M_STOP, M_DEFAULT, ProcessingFunction, &Rig.Hook);

	// 2. close the save queue and let the workers drain it (they push the move jobs);
	SaveQueue.Close();
	for (std::thread& Worker : SaveWorkers)
		Worker.join();

	// 3. close the move queue and gather the remote files on the host.
	capture::GatherImages(MoveQueue, MilApplication, MOVE_THREADS);


	for (size_t n = 0; n < Rigs.size(); n++)
	{
		MIL_INT FrameCount = 0;
		MIL_INT FrameMissed = 0;
		MIL_DOUBLE FrameRate = 0;
		MdigInquire(Rigs[n].Digitizer, M_PROCESS_FRAME_COUNT, &FrameCount);
		MdigInquire(Rigs[n].Digitizer, M_PROCESS_FRAME_MISSED, &FrameMissed);
		MdigInquire(Rigs[n].Digitizer, M_PROCESS_FRAME_RATE, &FrameRate);
		MosPrintf(MIL_TEXT("Camera %d: %d frames grabbed at %.1f frames/sec, %d frames missed\n"),
			(int)n + 1, (int)FrameCount, FrameRate, (int)FrameMissed);
	}

	MosPrintf(MIL_TEXT("Process Done!\n"));
	MosGetch();

	capture::FreeRigs(Rigs);
	MappFree(MilApplication);
	return 0;
}
