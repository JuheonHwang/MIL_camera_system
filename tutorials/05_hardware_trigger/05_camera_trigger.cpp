// 05_camera_trigger.cpp
//
// Chapter 05, example 2: the camera exposes on the pulse (camera trigger).
//
// What this adds over example 1:
//   The trigger is no longer used to release the grab. It starts the camera's
//   EXPOSURE. The pulse enters the frame grabber on AUX IO 4, the grabber
//   forwards it over the CoaXPress link ("LinkTrigger0") and the camera begins
//   one exposure per pulse. Feeding the same pulse to every grabber therefore
//   makes all cameras expose at the same moment. MdigGrab() just collects the
//   frame that results. Nothing is saved (see chapter 04).
//
//   Chapters 06 onwards call milcam::EnableExternalTrigger(), which wraps
//   exactly the calls written out below.
//
// Key MIL functions:
//   MdigControlFeature(TriggerSelector / TriggerMode / TriggerSource /
//                      TriggerActivation)         - camera side
//   MdigControl(M_IO_MODE + M_AUX_IO4, M_INPUT)   - grabber side
//   MdigControl(M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4)
//   MdigControl(M_GRAB_MODE, M_SYNCHRONOUS)
//
// At run time:
//   The program waits for pulses on AUX IO 4. Once every camera delivered a
//   frame the images appear; press any key to exit.
//
// Earlier experiments of the same idea also enabled a grab trigger and used
// M_TL_TRIGGER_ACTIVATION + M_TL_TRIGGER = M_ANY_EDGE (see example 4, which
// keeps that form). This file uses the most evolved version: synchronous grab
// mode and the default link-trigger activation.

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

// Edge on which the camera starts an exposure. Most experiments used
// "RisingEdge" (also the default of milcam::EnableExternalTrigger). The timer
// experiment (example 3) used "FallingEdge".
static const MIL_CONST_TEXT_PTR CAMERA_TRIGGER_ACTIVATION = MIL_TEXT("RisingEdge");

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

	// The most evolved experiment used 50 ms exposure at 15 fps; the frame rate
	// no longer matters once the camera is triggered, exposure still does.
	milcam::CameraSettings Settings;
	Settings.ExposureTime = 50000;
	for (int i = 0; i < CameraCount; i++)
		milcam::ApplyCameraSettings(MilDigitizer[i], Settings);

	for (int i = 0; i < CameraCount; i++)
		MilImage[i] = milcam::AllocMono8Buffer(MilSystem[i], MilDigitizer[i], M_IMAGE + M_DISP + M_GRAB);

	// ---- NEW: camera-side trigger ---------------------------------------------
	for (int i = 0; i < CameraCount; i++)
	{
		MIL_ID Digitizer = MilDigitizer[i];

		// The camera starts one exposure per pulse arriving on its link trigger.
		MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
		MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
		MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
		MdigControlFeature(Digitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, CAMERA_TRIGGER_ACTIVATION);
	}

	// ---- NEW: grabber-side trigger --------------------------------------------
	for (int i = 0; i < CameraCount; i++)
	{
		MIL_ID Digitizer = MilDigitizer[i];

		// MdigGrab() waits for the triggered frame instead of timing out.
		MdigControl(Digitizer, M_GRAB_MODE, M_SYNCHRONOUS);

		// Route the AUX pin to the CoaXPress link trigger. This is what sends
		// the pulse down the coax cable to the camera.
		MdigControl(Digitizer, M_IO_MODE + TRIGGER_INPUT, M_INPUT);
		MdigControl(Digitizer, M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
		MdigControl(Digitizer, M_IO_SOURCE + M_TL_TRIGGER, TRIGGER_INPUT);
	}

	MosPrintf(MIL_TEXT("All devices wait for the trigger...\n"));

	// Sequential grabs: camera N+1 is armed only after camera N returned, so if
	// the pulses keep coming, its frame may belong to a LATER pulse than camera
	// N's. For a strict one-pulse-one-frame guarantee arm all cameras at once
	// (example 4, 05_trigger_threads).
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
