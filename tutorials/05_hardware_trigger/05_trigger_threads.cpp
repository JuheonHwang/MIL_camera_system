// 05_trigger_threads.cpp
//
// Chapter 05, example 4: one host thread per camera, all armed together.
//
// What this adds over example 2:
//   MdigGrab() blocks. Called in a loop it arms the cameras one after the
//   other, so a later camera may miss the pulse an earlier camera just used.
//   Here every camera gets its own std::thread that calls MdigGrab(). The
//   threads are released together by a single key press, so all grabs are
//   armed before the first pulse is sent. Nothing is saved (see chapter 04).
//
//   The trigger set-up is the older "camera trigger + grab trigger" form used
//   by the first four-camera experiments: camera features, link trigger with
//   M_ANY_EDGE, and a grab trigger on the same AUX pin.
//
// Key MIL functions:
//   MdigGrab()  called from worker threads
//   MdigControl(M_TL_TRIGGER_ACTIVATION + M_TL_TRIGGER, M_ANY_EDGE)
//   MdigControl(M_GRAB_TRIGGER_SOURCE / _ACTIVATION / _STATE)
//
// At run time:
//   1. The program prints "Press any key to start the capture".
//   2. Press a key: every thread arms MdigGrab().
//   3. Send pulses on AUX IO 4. When all threads returned, the images appear;
//      press any key again to exit.

#include <mil.h>

#include <atomic>
#include <functional>
#include <thread>

#include "mil_common.h"

// Address of the second PC (runs the Distributed MIL server). Replace with yours.
static const MIL_STRING REMOTE_PC = MIL_TEXT("192.168.0.2");

static const std::vector<milcam::CameraSource> CAMERAS = {
	{ M_SYSTEM_RAPIXOCXP,                 M_DEV0, M_DEV3 },
	{ M_SYSTEM_RAPIXOCXP,                 M_DEV1, M_DEV3 },
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV0, M_DEV3 },
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV1, M_DEV3 },
};

// Grabber pin that receives the pulse.
static const MIL_INT64 TRIGGER_INPUT = M_AUX_IO4;

static const MIL_CONST_TEXT_PTR CAMERA_TRIGGER_ACTIVATION = MIL_TEXT("RisingEdge");

// Waits for the start signal, then blocks in MdigGrab() until the frame of the
// next trigger has been written to the buffer.
static void GrabWorker(MIL_ID MilDigitizer, MIL_ID MilImage, std::atomic<bool>& Start)
{
	while (!Start)
		std::this_thread::yield();
	MdigGrab(MilDigitizer, MilImage);
}

int MosMain(void)
{
	const int CameraCount = (int)CAMERAS.size();

	MIL_ID MilApplication = M_NULL;
	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	std::vector<MIL_ID> MilSystem(CameraCount);
	std::vector<MIL_ID> MilDisplay(CameraCount);
	std::vector<MIL_ID> MilDigitizer(CameraCount);
	std::vector<MIL_ID> MilImage(CameraCount);

	for (int i = 0; i < CameraCount; i++)
	{
		MilSystem[i] = milcam::AllocSystem(CAMERAS[i]);
		MilDisplay[i] = milcam::AllocDisplay(MilSystem[i]);
		MilDigitizer[i] = milcam::AllocDigitizer(MilSystem[i], CAMERAS[i]);
	}

	milcam::CameraSettings Settings;
	for (int i = 0; i < CameraCount; i++)
		milcam::ApplyCameraSettings(MilDigitizer[i], Settings);

	for (int i = 0; i < CameraCount; i++)
		MilImage[i] = milcam::AllocMono8Buffer(MilSystem[i], MilDigitizer[i], M_IMAGE + M_DISP + M_GRAB);

	// Camera side, as in example 2.
	for (int i = 0; i < CameraCount; i++)
	{
		MIL_ID Digitizer = MilDigitizer[i];
		MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
		MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
		MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
		MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, CAMERA_TRIGGER_ACTIVATION);
	}

	// Grabber side: link trigger on any edge, plus a grab trigger on the same pin.
	for (int i = 0; i < CameraCount; i++)
	{
		MIL_ID Digitizer = MilDigitizer[i];
		MdigControl(Digitizer, M_IO_MODE + TRIGGER_INPUT, M_INPUT);
		MdigControl(Digitizer, M_TL_TRIGGER_ACTIVATION + M_TL_TRIGGER, M_ANY_EDGE);
		MdigControl(Digitizer, M_IO_SOURCE + M_TL_TRIGGER, TRIGGER_INPUT);
		MdigControl(Digitizer, M_GRAB_TRIGGER_SOURCE, TRIGGER_INPUT);
		MdigControl(Digitizer, M_GRAB_TRIGGER_ACTIVATION, M_EDGE_RISING);
		MdigControl(Digitizer, M_GRAB_TRIGGER_STATE, M_ENABLE);
	}

	// ---- NEW: one grab thread per camera ----------------------------------------
	std::atomic<bool> Start(false);
	std::vector<std::thread> Workers;
	for (int i = 0; i < CameraCount; i++)
		Workers.emplace_back(GrabWorker, MilDigitizer[i], MilImage[i], std::ref(Start));

	MosPrintf(MIL_TEXT("Press any key to start the capture.\n"));
	MosGetch();
	Start = true;
	MosPrintf(MIL_TEXT("All devices wait for the trigger...\n"));

	// Join before touching the buffers: a thread is done once its frame is in.
	// (The original also called detach() after join(), which throws; dropped.)
	for (std::thread& Worker : Workers)
		Worker.join();

	for (int i = 0; i < CameraCount; i++)
	{
		MdispSelect(MilDisplay[i], MilImage[i]);
		MosPrintf(MIL_TEXT("Camera %d grabbed a %d x %d frame.\n"), i,
			(int)MdigInquire(MilDigitizer[i], M_SIZE_X, M_NULL),
			(int)MdigInquire(MilDigitizer[i], M_SIZE_Y, M_NULL));
	}

	MosPrintf(MIL_TEXT("Press any key to exit.\n"));
	MosGetch();

	for (int i = 0; i < CameraCount; i++)
	{
		MbufFree(MilImage[i]);
		MdispFree(MilDisplay[i]);
		MdigFree(MilDigitizer[i]);
		MsysFree(MilSystem[i]);
	}
	MappFree(MilApplication);

	return 0;
}
