// 05_trigger_timer.cpp
//
// Chapter 05, example 3: a grabber timer shapes the pulse (camera trigger
// through a timer).
//
// What this adds over example 2:
//   The raw pulse is not sent to the camera directly. It starts a hardware
//   TIMER inside the frame grabber (M_TIMER1). The timer waits a configurable
//   delay, then emits a pulse of a configurable duration, and THAT pulse is
//   routed to the CoaXPress link trigger. This lets you delay the exposure of
//   each camera or reshape a pulse that is too short or too long, without
//   touching the external pulse generator. Nothing is saved (see chapter 04).
//
// Signal path:
//   pulse -> AUX IO 4 -> M_TIMER1 (delay, duration) -> M_TL_TRIGGER -> camera
//   The same AUX IO 4 pulse also drives the grab trigger, as in the original.
//
// Key MIL functions:
//   MdigControl(M_TIMER_TRIGGER_SOURCE / M_TIMER_TRIGGER_ACTIVATION + M_TIMER1)
//   MdigControl(M_TIMER_DELAY / M_TIMER_DURATION + M_TIMER1)
//   MdigControl(M_IO_SOURCE + M_TL_TRIGGER, M_TIMER1)
//   MdigControl(M_TIMER_STATE + M_TIMER1, M_ENABLE)
//   MdigControl(M_GRAB_TRIGGER_SOURCE / _ACTIVATION / _STATE)
//
// At run time:
//   The program waits for pulses on AUX IO 4. Once every camera delivered a
//   frame the images appear; press any key to exit.
//
// The timer experiment had only one configuration, so its values are kept as
// they were. Check the MIL documentation for the unit of M_TIMER_DELAY and
// M_TIMER_DURATION before changing them.

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

// Grabber pin that receives the pulse that starts the timer.
static const MIL_INT64 TRIGGER_INPUT = M_AUX_IO4;

// Timer values, as in the original experiment.
static const int TIMER_DELAY = 100;
static const int TIMER_DURATION = 10000;

// This experiment used "FallingEdge" for the camera; examples 1-2 use rising.
// It reacts to the timer output, so match it to the timer pulse polarity.
static const MIL_CONST_TEXT_PTR CAMERA_TRIGGER_ACTIVATION = MIL_TEXT("FallingEdge");

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

	// Camera side: same as example 2, only the activation edge differs.
	for (int i = 0; i < CameraCount; i++)
	{
		MIL_ID Digitizer = MilDigitizer[i];
		MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
		MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
		MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
		MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, CAMERA_TRIGGER_ACTIVATION);
	}

	// ---- NEW: grabber timer between the AUX pin and the link trigger ------------
	for (int i = 0; i < CameraCount; i++)
	{
		MIL_ID Digitizer = MilDigitizer[i];

		MdigControl(Digitizer, M_IO_MODE + TRIGGER_INPUT, M_INPUT);

		// The incoming pulse starts the timer on its rising edge.
		MdigControl(Digitizer, M_TIMER_TRIGGER_SOURCE + M_TIMER1, TRIGGER_INPUT);
		MdigControl(Digitizer, M_TIMER_DELAY + M_TIMER1, TIMER_DELAY);
		MdigControl(Digitizer, M_TIMER_DURATION + M_TIMER1, TIMER_DURATION);
		MdigControl(Digitizer, M_TIMER_TRIGGER_ACTIVATION + M_TIMER1, M_EDGE_RISING);

		// The camera now sees the TIMER output, not the raw pulse.
		MdigControl(Digitizer, M_IO_SOURCE + M_TL_TRIGGER, M_TIMER1);

		// The timer does nothing until it is enabled.
		MdigControl(Digitizer, M_TIMER_STATE + M_TIMER1, M_ENABLE);

		// Grab trigger on the raw pulse, as in the original experiment.
		MdigControl(Digitizer, M_GRAB_TRIGGER_SOURCE, TRIGGER_INPUT);
		MdigControl(Digitizer, M_GRAB_TRIGGER_ACTIVATION, M_EDGE_RISING);
		MdigControl(Digitizer, M_GRAB_TRIGGER_STATE, M_ENABLE);
	}

	MosPrintf(MIL_TEXT("All devices wait for the trigger...\n"));

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
