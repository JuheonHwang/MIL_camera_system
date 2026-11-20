// 01 - Single camera, live view
//
// What this chapter shows:
//   The smallest complete MIL program for one Rapixo CXP frame grabber and one
//   CoaXPress camera: allocate, configure the camera, show live video, free.
//   Nothing is wrapped in helper functions on purpose, so every raw call is visible.
//
// Key MIL functions:
//   MappAlloc, MsysAlloc, MdispAlloc, MdigAlloc   - object hierarchy
//   MdigControlFeature / MdigInquireFeature       - read/write camera (GenICam) features
//   MdigControl / MdigInquire                     - grabber-side settings and properties
//   MbufAlloc2d, MbufClear, MdispSelect           - the image that is grabbed and shown
//   MdigGrabContinuous, MdigHalt                  - start / stop live grab
//
// At run time:
//   The camera description is printed, then a window shows live video.
//   Press any key to stop; the grabber's properties are then printed and
//   everything is freed. Press any key again to close the console.

#include <mil.h>

#include <cmath>

// ---- Configuration --------------------------------------------------------

static const MIL_INT SYSTEM_NUM = M_DEV0;      // which Rapixo CXP board in this PC
static const MIL_INT DIGITIZER_NUM = M_DEV3;   // which digitizer (connection) on that board

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
	MIL_ID MilSystem = M_NULL;
	MIL_ID MilDisplay = M_NULL;
	MIL_ID MilDigitizer = M_NULL;
	MIL_ID MilImage = M_NULL;

	// ---- Allocation: application -> system -> display -> digitizer ----------
	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);
	MsysAlloc(M_SYSTEM_RAPIXOCXP, SYSTEM_NUM, M_DEFAULT, &MilSystem);
	MdispAlloc(MilSystem, M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay);
	MdigAlloc(MilSystem, DIGITIZER_NUM, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer);

	// ---- Write camera features ---------------------------------------------
	// Camera features are written with MdigControlFeature; the type constant must
	// match the feature (integer, string or floating point).
	// Image size is also told to the frame grabber with M_SOURCE_SIZE_X/Y so that
	// the grabber expects the same size the camera now sends.
	MIL_INT64 Height = IMAGE_HEIGHT;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControl(MilDigitizer, M_SOURCE_SIZE_Y, Height);

	MIL_INT64 Width = IMAGE_WIDTH;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControl(MilDigitizer, M_SOURCE_SIZE_X, Width);

	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));

	MIL_DOUBLE ExposureTime = EXPOSURE_TIME;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);

	// Frame rate is derived from the exposure time (1 s / exposure), so the
	// camera is not asked for a rate it cannot reach.
	MIL_DOUBLE AcquisitionFrameRate = RoundDown(1000000 / ExposureTime, 2);
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);

	// ---- Read camera features back -----------------------------------------
	// Reading a string feature fills a MIL_STRING.
	MIL_STRING DeviceVendorName;
	MdigInquireFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("DeviceVendorName"), M_TYPE_STRING, DeviceVendorName);
	MosPrintf(MIL_TEXT("Device Vendor Name: %s\n"), DeviceVendorName.c_str());

	MIL_STRING DeviceModelName;
	MdigInquireFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("DeviceModelName"), M_TYPE_STRING, DeviceModelName);
	MosPrintf(MIL_TEXT("Device Model Name: %s\n"), DeviceModelName.c_str());

	MIL_STRING PixelFormat;
	MdigInquireFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, PixelFormat);
	MosPrintf(MIL_TEXT("Pixel Format: %s\n"), PixelFormat.c_str());

	MosPrintf(MIL_TEXT("\n"));

	// ---- Image buffer ------------------------------------------------------
	// Sized from the digitizer so it always matches the camera's current image.
	// M_GRAB lets the digitizer write into it, M_DISP lets the display show it.
	MbufAlloc2d(MilSystem,
		(MIL_INT)(MdigInquire(MilDigitizer, M_SIZE_X, M_NULL)),
		(MIL_INT)(MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL)),
		8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage);
	MbufClear(MilImage, 0L);

	MdispSelect(MilDisplay, MilImage);

	// ---- Live view ---------------------------------------------------------
	// Returns immediately; grabbing continues in the background until MdigHalt.
	MdigGrabContinuous(MilDigitizer, MilImage);

	MosGetch();

	MdigHalt(MilDigitizer);

	// ---- Grabber-side properties (MdigInquire) ------------------------------
	MIL_STRING Format;
	MdigInquire(MilDigitizer, M_FORMAT, Format);
	MosPrintf(MIL_TEXT("Format: %s\n"), Format.c_str());

	MIL_INT ChannelNum;
	MdigInquire(MilDigitizer, M_CHANNEL_NUM, &ChannelNum);
	MosPrintf(MIL_TEXT("Channel Num: %d\n"), (int)ChannelNum);

	MIL_INT DigNumber;
	MdigInquire(MilDigitizer, M_NUMBER, &DigNumber);
	switch (DigNumber)
	{
	case M_DEV0: MosPrintf(MIL_TEXT("digitizer number zero.\n")); break;
	case M_DEV1: MosPrintf(MIL_TEXT("digitizer number one.\n")); break;
	case M_DEV2: MosPrintf(MIL_TEXT("digitizer number two.\n")); break;
	case M_DEV3: MosPrintf(MIL_TEXT("digitizer number three.\n")); break;
	}

	MIL_DOUBLE SelectedFrameRate;
	MdigInquire(MilDigitizer, M_SELECTED_FRAME_RATE, &SelectedFrameRate);
	MosPrintf(MIL_TEXT("Selected Frame Rate: %f\n"), SelectedFrameRate);

	MIL_INT Sign;
	MdigInquire(MilDigitizer, M_SIGN, &Sign);
	if (Sign == M_UNSIGNED)
	{
		MosPrintf(MIL_TEXT("Sign: Unsigned\n"));
	}
	else
	{
		MosPrintf(MIL_TEXT("Sign: Signed\n"));
	}

	MIL_INT SizeBand;
	MdigInquire(MilDigitizer, M_SIZE_BAND, &SizeBand);
	MosPrintf(MIL_TEXT("Size Band: %d\n"), (int)SizeBand);

	MIL_INT SizeBit;
	MdigInquire(MilDigitizer, M_SIZE_BIT, &SizeBit);
	MosPrintf(MIL_TEXT("Size Bit: %d\n"), (int)SizeBit);

	MIL_INT SizeX;
	MIL_INT SizeY;
	MdigInquire(MilDigitizer, M_SIZE_X, &SizeX);
	MdigInquire(MilDigitizer, M_SIZE_Y, &SizeY);
	MosPrintf(MIL_TEXT("Size X & Y: %d, %d\n"), (int)SizeX, (int)SizeY);

	MIL_INT Type;
	MdigInquire(MilDigitizer, M_TYPE, &Type);
	MosPrintf(MIL_TEXT("Type: %d\n"), (int)Type);

	// ---- Free, in the reverse order of allocation ---------------------------
	MbufFree(MilImage);
	MdigFree(MilDigitizer);
	MdispFree(MilDisplay);
	MsysFree(MilSystem);
	MappFree(MilApplication);

	return 0;
}
