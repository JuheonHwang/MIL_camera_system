// 02 - Two cameras on one PC, live view
//
// What this chapter adds over chapter 01:
//   Two Rapixo CXP boards in the same PC, one camera on each. Every MIL object
//   (system, display, digitizer, buffer) becomes an array indexed by camera, and
//   the repeated calls become loops over a camera table.
//
// What changes with more than one board:
//   - Each board is its own MIL system (M_DEV0, M_DEV1), so MsysAlloc is called once per board.
//   - A display, digitizer and image buffer belong to ONE system. Each must be allocated
//     on the system of the camera it serves; they cannot be shared across boards.
//   - Camera settings are written to each digitizer separately.
//   - One MappAlloc is still enough for the whole program.
//
// Key MIL functions: same as chapter 01, now called per camera.
//
// At run time:
//   Camera information is printed for each board, two windows show live video.
//   Press any key to stop; everything is freed.

#include <mil.h>

#include <cmath>

// ---- Configuration --------------------------------------------------------

// One entry per camera: which board (system) and which digitizer on that board.
struct CameraSource
{
	MIL_INT SystemNum;
	MIL_INT DigitizerNum;
};

static const CameraSource CAMERAS[] = {
	{ M_DEV0, M_DEV3 },   // first board
	{ M_DEV1, M_DEV3 },   // second board
};
static const int CAMERA_COUNT = (int)(sizeof(CAMERAS) / sizeof(CAMERAS[0]));

static const MIL_INT64 IMAGE_WIDTH = 5120;
static const MIL_INT64 IMAGE_HEIGHT = 5120;
static const MIL_DOUBLE EXPOSURE_TIME = 60000;  // microseconds

// Keeps (Digits - 1) decimals, rounding down: RoundDown(16.666, 2) == 16.6.
static MIL_DOUBLE RoundDown(MIL_DOUBLE Value, int Digits)
{
	MIL_DOUBLE Scale = pow(10, Digits - 1);
	return floor(Value * Scale) / Scale;
}

int MosMain()
{
	MIL_ID MilApplication = M_NULL;
	MIL_ID MilSystem[CAMERA_COUNT];
	MIL_ID MilDisplay[CAMERA_COUNT];
	MIL_ID MilDigitizer[CAMERA_COUNT];
	MIL_ID MilImage[CAMERA_COUNT];

	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	// ---- Allocation, one board at a time -------------------------------------
	for (int i = 0; i < CAMERA_COUNT; i++)
	{
		MsysAlloc(M_SYSTEM_RAPIXOCXP, CAMERAS[i].SystemNum, M_DEFAULT, &MilSystem[i]);
		MdispAlloc(MilSystem[i], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[i]);
		MdigAlloc(MilSystem[i], CAMERAS[i].DigitizerNum, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[i]);
	}

	// ---- Write the same camera features to every camera ----------------------
	MIL_DOUBLE AcquisitionFrameRate = RoundDown(1000000 / EXPOSURE_TIME, 2);
	for (int i = 0; i < CAMERA_COUNT; i++)
	{
		MIL_INT64 Height = IMAGE_HEIGHT;
		MdigControlFeature(MilDigitizer[i], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
		MdigControl(MilDigitizer[i], M_SOURCE_SIZE_Y, Height);

		MIL_INT64 Width = IMAGE_WIDTH;
		MdigControlFeature(MilDigitizer[i], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
		MdigControl(MilDigitizer[i], M_SOURCE_SIZE_X, Width);

		MdigControlFeature(MilDigitizer[i], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));

		MIL_DOUBLE ExposureTime = EXPOSURE_TIME;
		MdigControlFeature(MilDigitizer[i], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);

		MdigControlFeature(MilDigitizer[i], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);
	}

	// ---- Read back and print each camera's identity --------------------------
	// M_NUMBER on the system tells which board the camera is plugged into.
	for (int i = 0; i < CAMERA_COUNT; i++)
	{
		MIL_INT Number;
		MsysInquire(MilSystem[i], M_NUMBER, &Number);
		switch (Number)
		{
		case M_DEV0: MosPrintf(MIL_TEXT("Camera %d: device information in the first board.\n"), i); break;
		case M_DEV1: MosPrintf(MIL_TEXT("Camera %d: device information in the second board.\n"), i); break;
		}

		MIL_STRING DeviceVendorName;
		MdigInquireFeature(MilDigitizer[i], M_FEATURE_VALUE, MIL_TEXT("DeviceVendorName"), M_TYPE_STRING, DeviceVendorName);
		MosPrintf(MIL_TEXT("Device Vendor Name: %s\n"), DeviceVendorName.c_str());

		MIL_STRING DeviceModelName;
		MdigInquireFeature(MilDigitizer[i], M_FEATURE_VALUE, MIL_TEXT("DeviceModelName"), M_TYPE_STRING, DeviceModelName);
		MosPrintf(MIL_TEXT("Device Model Name: %s\n"), DeviceModelName.c_str());

		MIL_STRING DeviceSerialNumber;
		MdigInquireFeature(MilDigitizer[i], M_FEATURE_VALUE, MIL_TEXT("DeviceSerialNumber"), M_TYPE_STRING, DeviceSerialNumber);
		MosPrintf(MIL_TEXT("Device Serial Number: %s\n"), DeviceSerialNumber.c_str());

		MIL_STRING PixelFormat;
		MdigInquireFeature(MilDigitizer[i], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, PixelFormat);
		MosPrintf(MIL_TEXT("Pixel Format: %s\n"), PixelFormat.c_str());

		MosPrintf(MIL_TEXT("\n"));
	}

	// ---- Image buffers: each one is allocated on its camera's own system -----
	for (int i = 0; i < CAMERA_COUNT; i++)
	{
		MbufAlloc2d(MilSystem[i],
			(MIL_INT)(MdigInquire(MilDigitizer[i], M_SIZE_X, M_NULL)),
			(MIL_INT)(MdigInquire(MilDigitizer[i], M_SIZE_Y, M_NULL)),
			8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[i]);
	}
	for (int i = 0; i < CAMERA_COUNT; i++)
		MbufClear(MilImage[i], 0L);
	for (int i = 0; i < CAMERA_COUNT; i++)
		MdispSelect(MilDisplay[i], MilImage[i]);

	// ---- Live view on all cameras ---------------------------------------------
	// MdigGrabContinuous returns at once, so both cameras run at the same time.
	for (int i = 0; i < CAMERA_COUNT; i++)
		MdigGrabContinuous(MilDigitizer[i], MilImage[i]);

	MosGetch();

	for (int i = 0; i < CAMERA_COUNT; i++)
		MdigHalt(MilDigitizer[i]);

	MosPrintf(MIL_TEXT("GrabContinuous Done!\n"));

	// ---- Free, in the reverse order of allocation ---------------------------
	for (int i = 0; i < CAMERA_COUNT; i++)
		MbufFree(MilImage[i]);
	for (int i = 0; i < CAMERA_COUNT; i++)
	{
		MdigFree(MilDigitizer[i]);
		MdispFree(MilDisplay[i]);
	}
	for (int i = 0; i < CAMERA_COUNT; i++)
		MsysFree(MilSystem[i]);
	MappFree(MilApplication);

	return 0;
}
