// threaded_save_queue.cpp
//
// What this step adds over chapter 06 (MdigProcess with multiple buffers):
//   Chapter 06 saved the image inside the MdigProcess hook. MbufSave of a
//   5120 x 5120 image is slow (and for a remote camera it also crosses the
//   network), so the hook cannot keep up with the camera and frames are missed.
//   Here the hook only clones the grabbed buffer and pushes the clone on a
//   queue; a pool of worker threads pops clones, calls MbufSave, and frees them.
//
// Key MIL functions:
//   MdigGetHookInfo(M_MODIFIED_BUFFER + M_BUFFER_ID)  which buffer was just grabbed
//   MbufClone(... M_COPY_SOURCE_DATA)                 private copy, so the grab buffer
//                                                     can be reused at once
//   MbufSave / MbufFree                               done in the workers, not the hook
//   MsysControl(M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD)
//
// Files are saved on the PC that owns the buffer. For a remote (Distributed MIL)
// system the file name is prefixed with "remote:///", so the file lands on the
// remote PC's disk. Moving those files back to the host is chapter 08.
//
// At run time:
//   - Wire the trigger to the grabbers' AUX input (chapter 05).
//   - Press a key to start MdigProcess, press a key again to stop.
//   - Files appear as C:/output_image/FFFF_CC.tiff on each PC
//     (FFFF = frame number, CC = camera number starting at 1).

#include <mil.h>

#include <atomic>
#include <thread>
#include <vector>

#include "mil_common.h"

// Address of the second PC (runs the Distributed MIL server). Replace with yours.
static const MIL_STRING REMOTE_PC = MIL_TEXT("192.168.0.2");

// All four cameras use digitizer M_DEV3 of their board, as in the originals.
static const std::vector<milcam::CameraSource> CAMERAS = {
	{ M_SYSTEM_RAPIXOCXP,                 M_DEV0, M_DEV3 },
	{ M_SYSTEM_RAPIXOCXP,                 M_DEV1, M_DEV3 },
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV0, M_DEV3 },
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV1, M_DEV3 },
};

static const int N_FRAME = 180;    // grab buffers per camera (MdigProcess ring)
static const int N_THREAD = 12;    // save workers
static const MIL_STRING OUTPUT_DIR = MIL_TEXT("C:/output_image/");
static const MIL_STRING REMOTE_PREFIX = MIL_TEXT("remote:///");

// One clone waiting to be written to disk.
struct SaveJob
{
	MIL_ID Image;       // the clone; the worker frees it
	bool Remote;        // buffer lives on a remote PC
	int FrameIndex;
	int CameraIndex;
};

// What the hook needs for one camera. The hook of every camera pushes on the
// same queue.
struct HookData
{
	std::atomic<int> ProcessedImageCount{ 0 };
	bool Remote = false;
	int CameraIndex = 0;
	milcam::WorkQueue<SaveJob>* Queue = nullptr;
};

// Hook: keep it short. Clone, push, return.
static MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr)
{
	HookData* Data = (HookData*)HookDataPtr;
	int FrameIndex = ++Data->ProcessedImageCount;

	MIL_ID GrabbedBuffer = M_NULL;
	MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &GrabbedBuffer);

	// The grab buffer goes back to MdigProcess as soon as the hook returns and
	// will be overwritten, so work on a copy. M_HOST_MEMORY + M_OFF_BOARD keeps
	// the copy in ordinary memory on the PC that owns the buffer.
	MIL_ID Clone = M_NULL;
	MbufClone(GrabbedBuffer, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_IMAGE + M_HOST_MEMORY + M_OFF_BOARD, M_COPY_SOURCE_DATA, &Clone);
	if (Clone == M_NULL)
		return 0;   // out of memory: this frame is lost, but the hook stays alive

	Data->Queue->Push(SaveJob{ Clone, Data->Remote, FrameIndex, Data->CameraIndex });
	return 0;
}

// Worker: runs until the queue is closed and empty.
static void SaveWorker(milcam::WorkQueue<SaveJob>& Queue)
{
	SaveJob Job;
	while (Queue.Pop(Job))   // blocks while the queue is empty
	{
		MIL_STRING FileName = milcam::FrameFileName(OUTPUT_DIR, Job.FrameIndex, Job.CameraIndex);
		if (Job.Remote)
			FileName = REMOTE_PREFIX + FileName;   // saved on the remote PC's disk

		MbufSave(FileName, Job.Image);
		MbufFree(Job.Image);
	}
}

int MosMain(void)
{
	const int CameraCount = (int)CAMERAS.size();
	const milcam::CameraSettings Settings = []
	{
		milcam::CameraSettings S;
		S.ExposureTime = 10000;
		S.AcquisitionFrameRate = 90;
		return S;
	}();

	MIL_ID MilApplication = M_NULL;
	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	std::vector<MIL_ID> MilSystem(CameraCount);
	std::vector<MIL_ID> MilDigitizer(CameraCount);
	std::vector<std::vector<MIL_ID>> GrabBuffers(CameraCount, std::vector<MIL_ID>(N_FRAME));
	std::vector<HookData> Hooks(CameraCount);
	milcam::WorkQueue<SaveJob> SaveQueue;

	for (int n = 0; n < CameraCount; n++)
	{
		MilSystem[n] = milcam::AllocSystem(CAMERAS[n]);
		// Let hooks of one system run on several threads.
		MsysControl(MilSystem[n], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
		MilDigitizer[n] = milcam::AllocDigitizer(MilSystem[n], CAMERAS[n]);
	}

	for (int n = 0; n < CameraCount; n++)
	{
		milcam::ApplyCameraSettings(MilDigitizer[n], Settings);
		milcam::EnableExternalTrigger(MilDigitizer[n]);
	}

	for (int n = 0; n < CameraCount; n++)
		for (int f = 0; f < N_FRAME; f++)
			GrabBuffers[n][f] = milcam::AllocMono8Buffer(MilSystem[n], MilDigitizer[n], M_IMAGE + M_GRAB);

	for (int n = 0; n < CameraCount; n++)
	{
		Hooks[n].Remote = milcam::IsRemoteSystem(MilSystem[n]);
		Hooks[n].CameraIndex = n + 1;
		Hooks[n].Queue = &SaveQueue;
		MdigControl(MilDigitizer[n], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
	}

	// Start the workers before the first frame can arrive. They simply block in
	// Pop() until the first clone is pushed.
	std::vector<std::thread> Workers;
	for (int n = 0; n < N_THREAD; n++)
		Workers.emplace_back(SaveWorker, std::ref(SaveQueue));

	MosPrintf(MIL_TEXT("Press any key to start the process...\n"));
	MosGetch();

	for (int n = 0; n < CameraCount; n++)
		MdigProcess(MilDigitizer[n], GrabBuffers[n].data(), N_FRAME, M_START, M_TRIGGER_FOR_FIRST_GRAB, ProcessingFunction, &Hooks[n]);

	MosPrintf(MIL_TEXT("Press any key to stop the process...\n"));
	MosGetch();

	// Shutdown order matters:
	//  1. Stop MdigProcess on EVERY camera. M_STOP returns after the last hook
	//     has finished, so nothing can push after this loop.
	//  2. Close the queue so workers that run dry leave Pop().
	//  3. Join the workers; they first drain the clones still waiting.
	for (int n = 0; n < CameraCount; n++)
		MdigProcess(MilDigitizer[n], GrabBuffers[n].data(), N_FRAME, M_STOP, M_DEFAULT, ProcessingFunction, &Hooks[n]);

	SaveQueue.Close();
	MosPrintf(MIL_TEXT("Waiting for the save threads to finish...\n"));
	for (std::thread& Worker : Workers)
		Worker.join();

	for (int n = 0; n < CameraCount; n++)
	{
		MIL_INT FrameCount = 0;
		MIL_INT FrameMissed = 0;
		MIL_DOUBLE FrameRate = 0;
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_COUNT, &FrameCount);
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_MISSED, &FrameMissed);
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_RATE, &FrameRate);
		MosPrintf(MIL_TEXT("Camera %d: %d frames grabbed at %.1f frames/sec, %d frames missed\n"),
			n + 1, (int)FrameCount, FrameRate, (int)FrameMissed);
	}

	MosPrintf(MIL_TEXT("Process done. Press any key to exit.\n"));
	MosGetch();

	for (int n = 0; n < CameraCount; n++)
		for (int f = 0; f < N_FRAME; f++)
			MbufFree(GrabBuffers[n][f]);

	for (int n = 0; n < CameraCount; n++)
	{
		MdigFree(MilDigitizer[n]);
		MsysFree(MilSystem[n]);
	}
	MappFree(MilApplication);

	return 0;
}
