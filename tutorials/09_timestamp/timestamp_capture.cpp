// timestamp_capture.cpp
//
// Chapter 09: put the per-frame hardware time stamp into the file name.
//
// What this adds over chapters 07/08 (threaded save queue, remote save and move):
//   Only the lines marked "NEW (09)". In the MdigProcess hook, the time stamp of
//   the grabbed frame is read with MdigGetHookInfo(..., M_TIME_STAMP, ...), carried
//   in the queue item, and appended to the file name:
//       <frame>_<camera>_<timestamp>.tiff      e.g. 0012_03_1234.567890.tiff
//   Nothing is printed and nothing is compared in code: the time stamp is written
//   into the name so that frames of different cameras (and different PCs) can be
//   matched or their timing checked afterwards, by sorting or diffing file names.
//
// Key MIL functions: MdigProcess, MdigGetHookInfo (M_MODIFIED_BUFFER + M_BUFFER_ID
//   and M_TIME_STAMP), MbufClone, MbufSave, MappFileOperation.
//
// Run time: the program starts MdigProcess on every camera (external trigger),
//   prints "Camera Ready Done!", and grabs until you press a key. It then stops,
//   moves the files saved on the remote PC to this PC, and prints frame statistics.
//   Press a key once more to free everything and exit.
//
// Local or remote: the CAMERAS table decides. Delete the remote rows to run on a
//   single PC; the code paths for remote buffers are then simply not taken.

#include <mil.h>
#include "mil_common.h"

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

static const int N_FRAME = 45;        // grab buffers per camera
static const int SAVE_THREADS = 4;
static const int MOVE_THREADS = 4;

static const MIL_STRING OUTPUT_DIR = MIL_TEXT("C:/output_image/");

static milcam::CameraSettings MakeSettings()
{
	milcam::CameraSettings Settings;
	Settings.ExposureTime = 30000;
	Settings.AcquisitionFrameRate = 30;
	Settings.Gain = 2;
	return Settings;
}

// ---------------------------------------------------------------------------
// Queue items
// ---------------------------------------------------------------------------

struct SaveItem
{
	MIL_ID Image;            // clone of the grabbed buffer; the worker frees it
	MIL_ID OwnerApp;         // application that owns the system the image lives on
	bool Remote;             // image lives on another PC
	int FrameIndex;          // 1, 2, 3, ... per camera
	int CameraIndex;         // 1-based, as in the original
	MIL_DOUBLE TimeStamp;    // NEW (09): time stamp of the frame, from the hook
};

struct MoveItem
{
	MIL_ID OwnerApp;         // remote application the file is on
	MIL_STRING FileName;     // path on the remote PC (and on the host after the move)
};

// Per-camera data handed to the hook through the MdigProcess user pointer.
struct HookData
{
	milcam::WorkQueue<SaveItem>* SaveQueue;
	MIL_ID OwnerApp;
	bool Remote;
	int CameraIndex;
	MIL_INT ProcessedImageCount;
};

// ---------------------------------------------------------------------------
// Hook: called by MdigProcess for every grabbed frame
// ---------------------------------------------------------------------------

MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr)
{
	HookData* Hook = (HookData*)HookDataPtr;
	MIL_ID GrabbedBuffer;
	MIL_ID CloneId;
	MIL_DOUBLE EventTimeStamp;   // NEW (09)

	Hook->ProcessedImageCount++;

	MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &GrabbedBuffer);

	// NEW (09): ask the same hook event for the time stamp of this frame. It must
	// be read here: the hook id is only valid while the hook function runs.
	MdigGetHookInfo(HookId, M_TIME_STAMP, &EventTimeStamp);

	// The grab buffer goes back to MdigProcess when the hook returns, so queue a copy.
	MbufClone(GrabbedBuffer, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_IMAGE + M_HOST_MEMORY + M_OFF_BOARD, M_COPY_SOURCE_DATA, &CloneId);

	SaveItem Item = { CloneId, Hook->OwnerApp, Hook->Remote, (int)Hook->ProcessedImageCount, Hook->CameraIndex, EventTimeStamp };
	Hook->SaveQueue->Push(Item);
	return 0;
}

// ---------------------------------------------------------------------------
// Workers
// ---------------------------------------------------------------------------

// NEW (09): "<frame>_<camera>_<timestamp>.tiff". The time stamp is printed with six
// decimals, the same text the original produced with to_string(double).
static MIL_STRING TimeStampedFileName(const SaveItem& Item)
{
	MIL_STRING_STREAM Suffix;
	Suffix << MIL_TEXT("_") << std::fixed << std::setprecision(6) << Item.TimeStamp << MIL_TEXT(".tiff");
	return milcam::FrameFileName(OUTPUT_DIR, Item.FrameIndex, Item.CameraIndex, Suffix.str());
}

static void SaveWorker(milcam::WorkQueue<SaveItem>& SaveQueue, milcam::WorkQueue<MoveItem>& MoveQueue)
{
	SaveItem Item;
	while (SaveQueue.Pop(Item))
	{
		MIL_STRING FileName = TimeStampedFileName(Item);

		if (Item.Remote)
		{
			// "remote:///" makes MbufSave write on the PC that owns the buffer.
			MbufSave((MIL_TEXT("remote:///") + FileName).c_str(), Item.Image);
			MbufFree(Item.Image);
			MoveQueue.Push({ Item.OwnerApp, FileName });
		}
		else
		{
			MbufSave(FileName.c_str(), Item.Image);
			MbufFree(Item.Image);
		}
	}
}

static void MoveWorker(milcam::WorkQueue<MoveItem>& MoveQueue, MIL_ID HostApp)
{
	MoveItem Item;
	while (MoveQueue.Pop(Item))
	{
		MappFileOperation(Item.OwnerApp, Item.FileName.c_str(), HostApp, Item.FileName.c_str(), M_FILE_MOVE, M_DEFAULT, M_NULL);
	}
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int MosMain(void)
{
	const int N_CAM = (int)CAMERAS.size();

	MIL_ID MilApplication = M_NULL;
	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);
	MappControl(MilApplication, M_ERROR, M_PRINT_DISABLE);
	MappControlMp(M_DEFAULT, M_MP_USE, M_DEFAULT, M_ENABLE, M_NULL);

	std::vector<MIL_ID> MilSystem(N_CAM);
	std::vector<MIL_ID> MilDigitizer(N_CAM);
	std::vector<std::vector<MIL_ID> > GrabBuffers(N_CAM, std::vector<MIL_ID>(N_FRAME));
	std::vector<HookData> Hooks(N_CAM);   // sized once: the hooks keep pointers into it

	milcam::WorkQueue<SaveItem> SaveQueue;
	milcam::WorkQueue<MoveItem> MoveQueue;

	for (int n = 0; n < N_CAM; n++)
	{
		MilSystem[n] = milcam::AllocSystem(CAMERAS[n]);
		MsysControl(MilSystem[n], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
		MilDigitizer[n] = milcam::AllocDigitizer(MilSystem[n], CAMERAS[n]);
	}

	const milcam::CameraSettings Settings = MakeSettings();
	for (int n = 0; n < N_CAM; n++)
	{
		milcam::ApplyCameraSettings(MilDigitizer[n], Settings);
		milcam::EnableExternalTrigger(MilDigitizer[n]);
	}

	for (int n = 0; n < N_CAM; n++)
		for (int f = 0; f < N_FRAME; f++)
			GrabBuffers[n][f] = milcam::AllocMono8Buffer(MilSystem[n], MilDigitizer[n], M_IMAGE + M_GRAB);

	for (int n = 0; n < N_CAM; n++)
	{
		Hooks[n].SaveQueue = &SaveQueue;
		Hooks[n].OwnerApp = (MIL_ID)MsysInquire(MilSystem[n], M_OWNER_APPLICATION, M_NULL);
		Hooks[n].Remote = milcam::IsRemoteSystem(MilSystem[n]);
		Hooks[n].CameraIndex = n + 1;
		Hooks[n].ProcessedImageCount = 0;

		MdigControl(MilDigitizer[n], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
	}

	for (int n = 0; n < N_CAM; n++)
		MdigProcess(MilDigitizer[n], GrabBuffers[n].data(), N_FRAME, M_START, M_DEFAULT, ProcessingFunction, &Hooks[n]);
	MosPrintf(MIL_TEXT("Camera Ready Done!\n"));

	std::vector<std::thread> SaveThreads;
	for (int n = 0; n < SAVE_THREADS; n++)
		SaveThreads.emplace_back(SaveWorker, std::ref(SaveQueue), std::ref(MoveQueue));

	MosPrintf(MIL_TEXT("Press Any Key to Stop Process...\n"));
	MosGetch();

	// Shutdown order: stop the producers, close the queue, then join the workers.
	for (int n = 0; n < N_CAM; n++)
		MdigProcess(MilDigitizer[n], GrabBuffers[n].data(), N_FRAME, M_STOP, M_DEFAULT, ProcessingFunction, &Hooks[n]);

	SaveQueue.Close();
	for (std::thread& Thread : SaveThreads)
		Thread.join();

	// All saves are done, so the move queue is complete: close it and let the move
	// workers drain it.
	MosPrintf(MIL_TEXT("Gathering Images...\n\n"));
	MoveQueue.Close();
	std::vector<std::thread> MoveThreads;
	for (int n = 0; n < MOVE_THREADS; n++)
		MoveThreads.emplace_back(MoveWorker, std::ref(MoveQueue), MilApplication);
	for (std::thread& Thread : MoveThreads)
		Thread.join();

	for (int n = 0; n < N_CAM; n++)
	{
		MIL_INT ProcessFrameCount = 0;
		MIL_INT ProcessFrameMissedCount = 0;
		MIL_DOUBLE ProcessFrameRate = 0;
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_COUNT, &ProcessFrameCount);
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_MISSED, &ProcessFrameMissedCount);
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_RATE, &ProcessFrameRate);
		MosPrintf(MIL_TEXT("%02dth Digitizer: %d frames grabbed at %.1f frames/sec, %d frames missed\n"),
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
