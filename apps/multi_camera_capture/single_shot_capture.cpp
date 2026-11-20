// single_shot_capture.cpp
//
// Finished application, variant 2: interactive capture. Each Enter grabs one
// synchronized set of frames (one per camera, 10 cameras on 5 PCs), saved on
// each PC and gathered on the host when you quit.
//
// Differs from continuous_capture.cpp: MdigGrab instead of MdigProcess, one
// capture thread per camera, a long exposure and 1 frame/s camera rate (the
// hardware trigger pulses are what decides when a frame is exposed).
//
// Key MIL functions: MdigGrab, MbufClear, MbufClone, MbufSave,
// MappFileOperation.
//
// At run time:
//   [Enter]  open a one-second grab window. Every camera thread calls MdigGrab,
//            which returns when the next trigger pulse has delivered a frame.
//   [Q]      quit: finish saving, move the files of the remote PCs to the host.
// Files are C:/output_image/FFFF_CC.tiff (FFFF = shot 0001.., CC = camera 01..10).
//
// How the original behaves (kept on purpose):
//   - The capture threads are started once and poll a flag. While the flag is
//     "wait" they sleep; while the one-second window is open they loop on
//     MdigGrab. A camera that gets two trigger pulses inside the window grabs
//     two frames, so shot numbers of different cameras can drift apart. Send
//     one trigger pulse per Enter to keep them in step.
//   - The shot number of a camera is its own grab counter (1-based).

#include <mil.h>

#include "capture_common.h"

// Camera features written to every camera.
static milcam::CameraSettings MakeSettings()
{
	milcam::CameraSettings Settings;
	Settings.ExposureTime = 999500;      // microseconds, almost one second
	Settings.AcquisitionFrameRate = 1;   // frames per second
	return Settings;
}

static const int GRAB_BUFFERS = 10;  // ring of buffers per camera, reused every 10 shots
static const int SAVE_THREADS = 4;
static const int MOVE_THREADS = 12;

static const MIL_INT KEY_ENTER = 13;
static const int GRAB_WINDOW_MS = 1000;  // how long one Enter keeps the grab loops open
static const int POLL_MS = 500;          // idle capture threads re-check the flag this often

// One thread per camera. While GrabWindowOpen is true it grabs; otherwise it
// sleeps, and leaves the loop when Quit is set.
static void CaptureWorker(capture::CameraRig& Rig, const std::atomic<bool>& GrabWindowOpen, const std::atomic<bool>& Quit)
{
	while (true)
	{
		if (!GrabWindowOpen)
		{
			if (Quit)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(POLL_MS));
			continue;
		}

		MIL_INT Shot = Rig.Hook.FrameCount++;       // 0-based
		MIL_ID Buffer = Rig.Buffers[Shot % GRAB_BUFFERS];

		MbufClear(Buffer, 0L);
		MdigGrab(Rig.Digitizer, Buffer);            // blocks until a triggered frame arrives
		capture::QueueFrameForSave(Rig.Hook, Buffer, (int)Shot + 1);
	}
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

	MosPrintf(MIL_TEXT("Press [Enter] to Capture One Image Each Camera...\n"));
	MosPrintf(MIL_TEXT("Press [Q] to Stop Capture Process...\n"));

	std::atomic<bool> GrabWindowOpen(false);
	std::atomic<bool> Quit(false);

	std::vector<std::thread> CaptureWorkers;
	for (capture::CameraRig& Rig : Rigs)
		CaptureWorkers.emplace_back(CaptureWorker, std::ref(Rig), std::cref(GrabWindowOpen), std::cref(Quit));

	std::vector<std::thread> SaveWorkers;
	for (int n = 0; n < SAVE_THREADS; n++)
		SaveWorkers.emplace_back(capture::SaveWorker, std::ref(SaveQueue), std::ref(MoveQueue));

	int ShotCount = 0;
	bool Capturing = true;
	while (Capturing)
	{
		MosPrintf(MIL_TEXT("Press Key you want...\n"));
		MIL_INT KeyPressed = MosGetch();

		if (KeyPressed == KEY_ENTER)
		{
			MosPrintf(MIL_TEXT("[Enter] Key Pressed\n"));
			GrabWindowOpen = true;
			MosSleep(GRAB_WINDOW_MS);
			GrabWindowOpen = false;
			ShotCount++;
			MosPrintf(MIL_TEXT("%d Image Captured!\n"), ShotCount);
		}

		if (KeyPressed == 'Q' || KeyPressed == 'q')
		{
			MosPrintf(MIL_TEXT("[Q] Key Pressed\n"));
			Capturing = false;
			Quit = true;
		}

		MosSleep(GRAB_WINDOW_MS);
	}

	// Shutdown order matters:
	// 1. capture threads end first, so nothing new is pushed to the save queue;
	for (std::thread& Worker : CaptureWorkers)
		Worker.join();

	// 2. close the save queue and let the workers drain it (they push the move jobs);
	SaveQueue.Close();
	for (std::thread& Worker : SaveWorkers)
		Worker.join();

	// 3. close the move queue and gather the remote files on the host.
	capture::GatherImages(MoveQueue, MilApplication, MOVE_THREADS);

	for (size_t n = 0; n < Rigs.size(); n++)
		MosPrintf(MIL_TEXT("Camera %d: %d frames grabbed\n"), (int)n + 1, (int)Rigs[n].Hook.FrameCount.load());

	MosPrintf(MIL_TEXT("Process Done!\n"));
	MosGetch();

	capture::FreeRigs(Rigs);
	MappFree(MilApplication);
	return 0;
}
