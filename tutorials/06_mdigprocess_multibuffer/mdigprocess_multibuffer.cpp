// Chapter 06: MdigProcess with a list of grab buffers
//
// What this step adds over chapter 05 (hardware trigger):
//   Chapter 05 grabs frames with a MdigGrab loop. Here the digitizers are
//   handed a LIST of buffers and MIL grabs into them continuously by itself.
//   For every finished frame MIL calls a hook function that we provide.
//   The main thread only has to start and stop the process.
//
// Key MIL functions:
//   MdigProcess            - M_START / M_STOP, buffer list, hook function, user data
//   MdigGetHookInfo        - inside the hook: which buffer was just filled
//   MsysControl            - M_MODIFIED_BUFFER_HOOK_MODE = M_MULTI_THREAD
//   MdigControl            - M_GRAB_FRAME_MISSED_RESET
//   MdigInquire            - M_PROCESS_FRAME_COUNT / _MISSED / _RATE
//   MbufExport             - writes the frame to a PNG file (inside the hook)
//
// Single buffer vs several buffers:
//   NUM_GRAB_BUFFERS = 1 gives the "single buffer" original, 15 the "multi
//   buffer" one. Nothing else differs between them. With one buffer MIL has
//   nowhere to put the next frame until the hook has returned; with several
//   it can keep grabbing while the hook is still busy with an earlier frame.
//
// Run time:
//   Press a key to start the process, send trigger pulses to the grabbers,
//   press a key again to stop. The per-camera statistics are printed, then
//   press a key once more to exit.

#include <mil.h>
#include "mil_common.h"

#include <filesystem>
#include <vector>

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// Address of the second PC (runs the Distributed MIL server). Replace with yours.
static const MIL_STRING REMOTE_PC = MIL_TEXT("192.168.0.2");

static const std::vector<milcam::CameraSource> CAMERAS = {
	{ M_SYSTEM_RAPIXOCXP,                 M_DEV0, M_DEV3 },
	{ M_SYSTEM_RAPIXOCXP,                 M_DEV1, M_DEV3 },
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV0, M_DEV3 },
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV1, M_DEV3 },
};

// Number of grab buffers per camera: 1 = single buffer, 15 = multi buffer.
static const MIL_INT NUM_GRAB_BUFFERS = 15;

// true: the hook saves every frame with MbufExport (slow at 5120x5120, see README).
// false: the hook only counts frames, so you can compare the statistics.
static const bool SAVE_IN_HOOK = true;

static const MIL_STRING OUTPUT_DIR = MIL_TEXT("C:/output_image/");

// The originals compute 1000000 / ExposureTime and round it down to 0.1 fps.
static milcam::CameraSettings MakeSettings()
{
	milcam::CameraSettings Settings;
	Settings.ExposureTime = 60000;
	Settings.AcquisitionFrameRate = 16.6;
	return Settings;
}

// ---------------------------------------------------------------------------
// Hook
// ---------------------------------------------------------------------------

// One of these per camera, passed to MdigProcess and handed back to the hook.
struct HookData
{
	MIL_INT ProcessedImageCount;
	int CameraIndex;        // 1-based, used in the file name
	MIL_STRING OutputDir;
};

// Called by MIL once for every grabbed frame. While it runs, the buffer it
// received is in use; anything slow here delays the return of that buffer.
MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr)
{
	HookData* Data = (HookData*)HookDataPtr;
	Data->ProcessedImageCount++;

	// The buffer that holds the frame that triggered this call.
	MIL_ID ModifiedBuffer;
	MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &ModifiedBuffer);

	if (SAVE_IN_HOOK)
	{
		MIL_STRING FileName = milcam::FrameFileName(Data->OutputDir, (int)Data->ProcessedImageCount, Data->CameraIndex, MIL_TEXT(".png"));
		MbufExport(FileName, M_PNG, ModifiedBuffer);
	}

	return 0;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int MosMain(void)
{
	const int NumCameras = (int)CAMERAS.size();
	const milcam::CameraSettings Settings = MakeSettings();

	MIL_ID MilApplication = M_NULL;
	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	std::vector<MIL_ID> MilSystem(NumCameras);
	std::vector<MIL_ID> MilDigitizer(NumCameras);
	std::vector<std::vector<MIL_ID>> GrabBuffers(NumCameras, std::vector<MIL_ID>(NUM_GRAB_BUFFERS));
	std::vector<HookData> UserHookData(NumCameras);

	for (int n = 0; n < NumCameras; n++)
	{
		MilSystem[n] = milcam::AllocSystem(CAMERAS[n]);

		// Run the hook of this system on its own thread, so the hooks of
		// different cameras do not wait for each other.
		MsysControl(MilSystem[n], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);

		MilDigitizer[n] = milcam::AllocDigitizer(MilSystem[n], CAMERAS[n]);
	}

	for (int n = 0; n < NumCameras; n++)
		milcam::ApplyCameraSettings(MilDigitizer[n], Settings);

	for (int n = 0; n < NumCameras; n++)
		milcam::EnableExternalTrigger(MilDigitizer[n]);

	// The buffer list MdigProcess grabs into, one list per camera.
	for (int n = 0; n < NumCameras; n++)
		for (MIL_INT i = 0; i < NUM_GRAB_BUFFERS; i++)
			GrabBuffers[n][i] = milcam::AllocMono8Buffer(MilSystem[n], MilDigitizer[n], M_IMAGE + M_GRAB);

	std::filesystem::create_directories(OUTPUT_DIR);

	for (int n = 0; n < NumCameras; n++)
	{
		UserHookData[n].ProcessedImageCount = 0;
		UserHookData[n].CameraIndex = n + 1;
		UserHookData[n].OutputDir = OUTPUT_DIR;
	}

	MosPrintf(MIL_TEXT("Press Any Key to Start Process...\n"));
	MosGetch();

	// M_START returns at once; frames are delivered to the hook from now on.
	// M_TRIGGER_FOR_FIRST_GRAB: the first frame waits for a hardware trigger.
	for (int n = 0; n < NumCameras; n++)
		MdigProcess(MilDigitizer[n], GrabBuffers[n].data(), NUM_GRAB_BUFFERS, M_START, M_TRIGGER_FOR_FIRST_GRAB, ProcessingFunction, &UserHookData[n]);

	// Start counting missed frames from zero.
	for (int n = 0; n < NumCameras; n++)
		MdigControl(MilDigitizer[n], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);

	MosPrintf(MIL_TEXT("Press Any Key to Stop Process...\n"));
	MosGetch();

	for (int n = 0; n < NumCameras; n++)
		MdigProcess(MilDigitizer[n], GrabBuffers[n].data(), NUM_GRAB_BUFFERS, M_STOP, M_DEFAULT, ProcessingFunction, &UserHookData[n]);

	// Statistics MIL kept for the process that just ended.
	for (int n = 0; n < NumCameras; n++)
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

	for (int n = 0; n < NumCameras; n++)
		for (MIL_INT i = 0; i < NUM_GRAB_BUFFERS; i++)
			MbufFree(GrabBuffers[n][i]);

	for (int n = 0; n < NumCameras; n++)
		MdigFree(MilDigitizer[n]);
	for (int n = 0; n < NumCameras; n++)
		MsysFree(MilSystem[n]);
	MappFree(MilApplication);

	return 0;
}
