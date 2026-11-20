// 08 - Save on each PC, then move the files to the host
//
// What this step adds over chapter 07 (threaded save queue):
//   Chapter 07 saves every grabbed frame on a worker thread. When a camera sits
//   on a remote PC (Distributed MIL), the file name is prefixed with
//   "remote:///" so the file is written to the remote PC's own disk and the
//   25 MB frame never crosses the network during capture. This chapter then
//   brings those files back to the host, AFTER capture, with a second queue
//   and a second pool of worker threads.
//
// Key MIL functions (new here):
//   MsysInquire(system, M_OWNER_APPLICATION, M_NULL)
//       Returns the id of the MIL application that owns a system. For a system
//       on a remote PC this is the application running on that PC.
//   MappFileOperation(srcApp, srcFile, dstApp, dstFile, M_FILE_MOVE, ...)
//       Moves a file from one application's computer to another's.
// Already known: MdigProcess, MbufClone, MbufSave, MsysInquire(M_DISTRIBUTED_MIL_TYPE).
//
// Data flow:
//   MdigProcess hook --clone--> save queue --N save workers--> MbufSave
//        (remote cameras only)                       |
//                                                    +--> move queue --M move workers--> MappFileOperation
//
// Shutdown order matters (see the end of MosMain):
//   1. stop MdigProcess on every camera  (no more hook calls, no more clones)
//   2. Close() the save queue, join the save workers
//   3. Close() the move queue (the save workers were its only producers),
//      start/join the move workers, and time them
//
// At run time:
//   - Press a key to start grabbing (frames wait for the hardware trigger).
//   - Press a key to stop. The program then prints "Gathering Images..." and,
//     when every remote file is on the host, how many seconds the move took.
//   - Press a key to exit.

#include <mil.h>
#include "mil_common.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// Address of the second PC (runs the Distributed MIL server). Replace with yours.
static const MIL_STRING REMOTE_PC = MIL_TEXT("192.168.0.2");

static const std::vector<milcam::CameraSource> CAMERAS = {
	{ M_SYSTEM_RAPIXOCXP,                 M_DEV0, M_DEV0 },
	{ M_SYSTEM_RAPIXOCXP,                 M_DEV1, M_DEV0 },
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV0, M_DEV0 },
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV1, M_DEV0 },
};

static const MIL_STRING OUTPUT_DIR = MIL_TEXT("C:/output_image/");  // same path on every PC
static const MIL_INT N_BUFFERS = 180;       // grab buffers per camera
static const int SAVE_THREADS = 12;
static const int MOVE_THREADS = 8;

static milcam::CameraSettings MakeSettings()
{
	milcam::CameraSettings Settings;
	Settings.ExposureTime = 10000;
	Settings.AcquisitionFrameRate = 90;
	return Settings;
}

// ---------------------------------------------------------------------------
// Queue items and shared state
// ---------------------------------------------------------------------------

// One cloned frame waiting to be saved.
struct SaveItem
{
	MIL_ID Image;           // clone made in the hook; the save worker frees it
	MIL_ID OwnerApp;        // application that owns the camera's system
	bool Remote;            // true if the camera is on another PC
	int FrameIndex;
	int CameraIndex;
};

// One file sitting on a remote PC, waiting to be moved to the host.
struct MoveItem
{
	MIL_ID RemoteApp;       // application on the PC that holds the file
	MIL_STRING FileName;    // path as seen on that PC (no "remote:///" prefix)
};

// Per-camera data handed to the MdigProcess hook.
struct HookData
{
	std::atomic<int> FrameCount;
	MIL_ID OwnerApp;
	bool Remote;
	int CameraIndex;
	milcam::WorkQueue<SaveItem>* SaveQueue;

	HookData() : FrameCount(0), OwnerApp(M_NULL), Remote(false), CameraIndex(0), SaveQueue(nullptr) {}
};

// ---------------------------------------------------------------------------
// Hook: runs for every grabbed frame
// ---------------------------------------------------------------------------

MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr)
{
	HookData* Data = (HookData*)HookDataPtr;
	MIL_ID GrabbedBuffer = M_NULL;
	MIL_ID Clone = M_NULL;

	int FrameIndex = ++Data->FrameCount;

	MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &GrabbedBuffer);

	// The grab buffer goes back to MdigProcess as soon as the hook returns, so
	// the worker threads get their own copy.
	MbufClone(GrabbedBuffer, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_IMAGE + M_HOST_MEMORY + M_OFF_BOARD, M_COPY_SOURCE_DATA, &Clone);
	if (Clone == M_NULL)
		return 0;   // out of memory: this frame is lost, but the hook stays alive

	Data->SaveQueue->Push({ Clone, Data->OwnerApp, Data->Remote, FrameIndex, Data->CameraIndex });
	return 0;
}

// ---------------------------------------------------------------------------
// Workers
// ---------------------------------------------------------------------------

// Pops clones, saves them, frees them. Remote files are announced on the move queue.
static void SaveWorker(milcam::WorkQueue<SaveItem>& SaveQueue, milcam::WorkQueue<MoveItem>& MoveQueue)
{
	SaveItem Item;
	while (SaveQueue.Pop(Item))
	{
		MIL_STRING FileName = milcam::FrameFileName(OUTPUT_DIR, Item.FrameIndex, Item.CameraIndex);

		if (Item.Remote)
		{
			// "remote:///" makes MbufSave write on the PC that owns the buffer.
			MbufSave(MIL_STRING(MIL_TEXT("remote:///")) + FileName, Item.Image);
			MbufFree(Item.Image);

			// Remember where the file lives; the move phase brings it back later.
			MoveQueue.Push({ Item.OwnerApp, FileName });
		}
		else
		{
			MbufSave(FileName, Item.Image);
			MbufFree(Item.Image);
		}
	}
}

// Pops file records and moves each file from its remote PC to the host.
static void MoveWorker(milcam::WorkQueue<MoveItem>& MoveQueue, MIL_ID HostApp)
{
	MoveItem Item;
	while (MoveQueue.Pop(Item))
	{
		// Source: the remote application and the path on its disk.
		// Destination: the host application, same path. M_FILE_MOVE deletes the
		// source after the copy, so the remote disk does not fill up.
		MappFileOperation(Item.RemoteApp, Item.FileName, HostApp, Item.FileName, M_FILE_MOVE, M_DEFAULT, M_NULL);
	}
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int MosMain(void)
{
	const size_t N_CAMERAS = CAMERAS.size();

	MIL_ID MilApplication = M_NULL;
	std::vector<MIL_ID> MilSystem(N_CAMERAS, M_NULL);
	std::vector<MIL_ID> MilDigitizer(N_CAMERAS, M_NULL);
	std::vector<std::vector<MIL_ID> > GrabBuffers(N_CAMERAS, std::vector<MIL_ID>(N_BUFFERS, M_NULL));
	std::vector<HookData> Hooks(N_CAMERAS);

	milcam::WorkQueue<SaveItem> SaveQueue;
	milcam::WorkQueue<MoveItem> MoveQueue;

	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);
	MappControlMp(M_DEFAULT, M_MP_USE, M_DEFAULT, M_ENABLE, M_NULL);

	// Allocate systems and digitizers.
	for (size_t c = 0; c < N_CAMERAS; c++)
	{
		MilSystem[c] = milcam::AllocSystem(CAMERAS[c]);
		MsysControl(MilSystem[c], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
		MilDigitizer[c] = milcam::AllocDigitizer(MilSystem[c], CAMERAS[c]);
	}

	// Camera settings, then grab buffers, then trigger routing (original order).
	const milcam::CameraSettings Settings = MakeSettings();
	for (size_t c = 0; c < N_CAMERAS; c++)
		milcam::ApplyCameraSettings(MilDigitizer[c], Settings);

	for (MIL_INT n = 0; n < N_BUFFERS; n++)
		for (size_t c = 0; c < N_CAMERAS; c++)
			GrabBuffers[c][n] = milcam::AllocMono8Buffer(MilSystem[c], MilDigitizer[c], M_IMAGE + M_GRAB);

	for (size_t c = 0; c < N_CAMERAS; c++)
		milcam::EnableExternalTrigger(MilDigitizer[c]);

	for (size_t c = 0; c < N_CAMERAS; c++)
	{
		Hooks[c].FrameCount = 0;
		Hooks[c].Remote = milcam::IsRemoteSystem(MilSystem[c]);
		// New in this chapter: the application that owns this system. For a
		// remote system it is the application on the remote PC, which is what
		// MappFileOperation needs as the file's source.
		Hooks[c].OwnerApp = (MIL_ID)MsysInquire(MilSystem[c], M_OWNER_APPLICATION, M_NULL);
		Hooks[c].CameraIndex = (int)c + 1;
		Hooks[c].SaveQueue = &SaveQueue;
	}

	for (size_t c = 0; c < N_CAMERAS; c++)
		MdigControl(MilDigitizer[c], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);

	MosPrintf(MIL_TEXT("Press Any Key to Start Process...\n"));
	MosGetch();

	for (size_t c = 0; c < N_CAMERAS; c++)
		MdigProcess(MilDigitizer[c], GrabBuffers[c].data(), N_BUFFERS, M_START, M_TRIGGER_FOR_FIRST_GRAB, ProcessingFunction, &Hooks[c]);

	// Save workers run during capture.
	std::vector<std::thread> SaveWorkers;
	for (int n = 0; n < SAVE_THREADS; n++)
		SaveWorkers.emplace_back(SaveWorker, std::ref(SaveQueue), std::ref(MoveQueue));

	MosPrintf(MIL_TEXT("Press Any Key to Stop Process...\n"));
	MosGetch();

	// 1. Stop grabbing on every camera: the hook can no longer push to SaveQueue.
	for (size_t c = 0; c < N_CAMERAS; c++)
		MdigProcess(MilDigitizer[c], GrabBuffers[c].data(), N_BUFFERS, M_STOP, M_DEFAULT, ProcessingFunction, &Hooks[c]);

	// 2. No more producers for the save queue. Workers drain what is left, then exit.
	SaveQueue.Close();
	for (std::thread& Worker : SaveWorkers)
		Worker.join();

	// 3. The save workers were the only producers of the move queue; it is
	//    complete now. Move the files with a second pool and time it.
	MoveQueue.Close();

	MosPrintf(MIL_TEXT("Gathering Images...\n"));
	const std::chrono::steady_clock::time_point MoveStart = std::chrono::steady_clock::now();

	std::vector<std::thread> MoveWorkers;
	for (int n = 0; n < MOVE_THREADS; n++)
		MoveWorkers.emplace_back(MoveWorker, std::ref(MoveQueue), MilApplication);
	for (std::thread& Worker : MoveWorkers)
		Worker.join();

	const std::chrono::duration<double> MoveSeconds = std::chrono::steady_clock::now() - MoveStart;
	MosPrintf(MIL_TEXT("Move took %.2f seconds\n"), MoveSeconds.count());

	// Statistics per camera.
	for (size_t c = 0; c < N_CAMERAS; c++)
	{
		MIL_INT FrameCount = 0;
		MIL_INT FrameMissed = 0;
		MIL_DOUBLE FrameRate = 0;
		MdigInquire(MilDigitizer[c], M_PROCESS_FRAME_COUNT, &FrameCount);
		MdigInquire(MilDigitizer[c], M_PROCESS_FRAME_MISSED, &FrameMissed);
		MdigInquire(MilDigitizer[c], M_PROCESS_FRAME_RATE, &FrameRate);
		MosPrintf(MIL_TEXT("Camera %d: %d frames grabbed at %.1f frames/sec, %d frames missed\n"),
			(int)c + 1, (int)FrameCount, FrameRate, (int)FrameMissed);
	}

	MosPrintf(MIL_TEXT("Process Done!\n"));
	MosGetch();

	for (size_t c = 0; c < N_CAMERAS; c++)
		for (MIL_INT n = 0; n < N_BUFFERS; n++)
			MbufFree(GrabBuffers[c][n]);
	for (size_t c = 0; c < N_CAMERAS; c++)
		MdigFree(MilDigitizer[c]);
	for (size_t c = 0; c < N_CAMERAS; c++)
		MsysFree(MilSystem[c]);
	MappFree(MilApplication);

	return 0;
}
