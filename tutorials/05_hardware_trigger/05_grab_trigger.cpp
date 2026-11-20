// 05_grab_trigger.cpp
//
// Chapter 05, example 1: the frame grabber waits for the pulse (grab trigger).
//
// What this adds over chapter 04:
//   Until now MdigGrab() returned as soon as the camera delivered a frame. Here
//   the camera keeps free-running, and the FRAME GRABBER holds back MdigGrab()
//   until an electrical pulse arrives on one of its auxiliary inputs
//   (M_AUX_IO4). The same pulse is wired to every grabber, so all cameras
//   return a frame for the same event. Images are NOT saved in this chapter
//   (chapter 04 covers saving): they are shown on the displays and a line is
//   printed for each camera.
//
// Key MIL functions:
//   MdigControl(M_IO_MODE + M_AUX_IO4, M_INPUT)   - make AUX IO 4 an input pin
//   MdigControl(M_GRAB_TRIGGER_SOURCE / _ACTIVATION / _STATE)
//   MdigGrab()                                    - now blocks until the pulse
//   MdispSelect()                                 - show the grabbed buffer
//
// At run time:
//   The program prints "All devices wait for the trigger..." and blocks. Send a
//   pulse on AUX IO 4 of every grabber (pulse generator, see README). Once all
//   cameras have returned a frame, the images appear; press any key to exit.
//
// Two ways to wire the grab trigger are in this file, selected by
// ROUTE_VIA_LINK_TRIGGER. false (default) is the direct form. true is the form
// from the "burst trigger" experiment: the pulse is first routed to the
// CoaXPress link trigger and the grab trigger listens to that instead.

#include <mil.h>

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

// Which pulse edge releases MdigGrab(). The most evolved experiment ("trigger &
// grab", four cameras) used the falling edge; the earliest single-camera
// experiment used M_EDGE_RISING. Pick the one that matches your pulse source.
static const MIL_INT64 GRAB_TRIGGER_ACTIVATION = M_EDGE_FALLING;

// false: M_GRAB_TRIGGER_SOURCE = the AUX pin itself.
// true : AUX pin -> M_TL_TRIGGER -> M_GRAB_TRIGGER_SOURCE (burst experiment,
//        rising edge, camera trigger features left untouched).
static const bool ROUTE_VIA_LINK_TRIGGER = false;

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

	// ---- NEW: trigger configuration -------------------------------------------
	for (int i = 0; i < CameraCount; i++)
	{
		MIL_ID Digitizer = MilDigitizer[i];

		if (!ROUTE_VIA_LINK_TRIGGER)
		{
			// The camera itself must NOT wait for a trigger: it free-runs and
			// the grabber decides which frame to keep.
			MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
			MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("Off"));

			// The AUX pin must be an input before it can be a trigger source.
			MdigControl(Digitizer, M_IO_MODE + TRIGGER_INPUT, M_INPUT);

			// Grab trigger: MdigGrab() only completes after this pulse edge.
			MdigControl(Digitizer, M_GRAB_TRIGGER_SOURCE, TRIGGER_INPUT);
			MdigControl(Digitizer, M_GRAB_TRIGGER_ACTIVATION, GRAB_TRIGGER_ACTIVATION);
			MdigControl(Digitizer, M_GRAB_TRIGGER_STATE, M_ENABLE);
		}
		else
		{
			// Same pulse, but listened to through the link-trigger signal.
			MdigControl(Digitizer, M_TL_TRIGGER_ACTIVATION + M_TL_TRIGGER, M_EDGE_RISING);
			MdigControl(Digitizer, M_IO_SOURCE + M_TL_TRIGGER, TRIGGER_INPUT);
			MdigControl(Digitizer, M_GRAB_TRIGGER_SOURCE, M_TL_TRIGGER);
			MdigControl(Digitizer, M_GRAB_TRIGGER_ACTIVATION, M_EDGE_RISING);
			MdigControl(Digitizer, M_GRAB_TRIGGER_STATE, M_ENABLE);
		}
	}

	MosPrintf(MIL_TEXT("All devices wait for the trigger...\n"));

	// One after the other: each call blocks until its own grab trigger fires.
	// Because the cameras are grabbed sequentially, camera N+1 is only armed
	// after camera N returned, so it may need the NEXT pulse. Example 4
	// (05_trigger_threads) arms all cameras at once.
	for (int i = 0; i < CameraCount; i++)
		MdigGrab(MilDigitizer[i], MilImage[i]);

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
