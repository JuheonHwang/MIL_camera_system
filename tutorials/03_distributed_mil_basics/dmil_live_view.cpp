// 03b - Distributed MIL: live view of local and remote cameras together
//
// What this chapter adds over chapter 02:
//   Four cameras in one program: two on boards in this PC and two on boards in
//   another PC, reached through Distributed MIL. Once a system is allocated
//   with a dmiltcp:// descriptor, the rest of the code (display, digitizer,
//   features, buffer, grab) is identical to the local case; only the camera
//   table changes. Run dmil_allocation_check first.
//
// Key MIL functions: those of chapter 02, plus
//   MsysAlloc with a dmiltcp:// descriptor
//   MsysInquire (M_DISTRIBUTED_MIL_TYPE) - local or remote
//
// At run time:
//   Camera information is printed for all four cameras, four windows show live video.
//   Press any key to stop; everything is freed.

#include <mil.h>

#include <cmath>

// ---- Configuration --------------------------------------------------------

// Address of the second PC (runs the Distributed MIL server). Replace with yours.
static const MIL_STRING REMOTE_PC = MIL_TEXT("192.168.0.2");
static const int REMOTE_PORT = 57010;

struct CameraSource
{
	MIL_STRING SystemDescriptor;   // M_SYSTEM_RAPIXOCXP or a dmiltcp:// descriptor
	MIL_INT SystemNum;             // board index on the PC that owns the board
	MIL_INT DigitizerNum;          // digitizer index on that board
};

static MIL_STRING RemoteRapixoCxp(const MIL_STRING& HostAddress, int Port)
{
	MIL_STRING_STREAM Stream;
	Stream << MIL_TEXT("dmiltcp://") << HostAddress << MIL_TEXT(":") << Port << MIL_TEXT("/M_SYSTEM_RAPIXOCXP");
	return Stream.str();
}

static const MIL_INT64 IMAGE_WIDTH = 5120;
static const MIL_INT64 IMAGE_HEIGHT = 5120;
static const MIL_DOUBLE EXPOSURE_TIME = 60000;  // microseconds

// Keeps (Digits - 1) decimals, rounding down: RoundDown(16.666, 2) == 16.6.
static MIL_DOUBLE RoundDown(MIL_DOUBLE Value, int Digits)
{
	MIL_DOUBLE Scale = pow(10, Digits - 1);
	return floor(Value * Scale) / Scale;
}

static const int CAMERA_COUNT = 4;

int MosMain()
{
	const MIL_STRING RemoteDescriptor = RemoteRapixoCxp(REMOTE_PC, REMOTE_PORT);

	const CameraSource CAMERAS[CAMERA_COUNT] = {
		{ M_SYSTEM_RAPIXOCXP, M_DEV0, M_DEV3 },   // this PC, first board
		{ M_SYSTEM_RAPIXOCXP, M_DEV1, M_DEV3 },   // this PC, second board
		{ RemoteDescriptor,   M_DEV0, M_DEV3 },   // other PC, first board
		{ RemoteDescriptor,   M_DEV1, M_DEV3 },   // other PC, second board
	};

	MIL_ID MilApplication = M_NULL;
	MIL_ID MilSystem[CAMERA_COUNT];
	MIL_ID MilDisplay[CAMERA_COUNT];
	MIL_ID MilDigitizer[CAMERA_COUNT];
	MIL_ID MilImage[CAMERA_COUNT];

	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	// ---- Allocation ----------------------------------------------------------
	// Display, digitizer and buffer are allocated on the same system as the camera,
	// so for a remote camera they are created through that remote system.
	for (int i = 0; i < CAMERA_COUNT; i++)
	{
		MsysAlloc(CAMERAS[i].SystemDescriptor.c_str(), CAMERAS[i].SystemNum, M_DEFAULT, &MilSystem[i]);
		MdispAlloc(MilSystem[i], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[i]);
		MdigAlloc(MilSystem[i], CAMERAS[i].DigitizerNum, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[i]);
	}

	// ---- Camera features: identical for local and remote cameras --------------
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
	for (int i = 0; i < CAMERA_COUNT; i++)
	{
		bool IsRemote = (MsysInquire(MilSystem[i], M_DISTRIBUTED_MIL_TYPE, M_NULL) == M_DMIL_REMOTE);

		MIL_INT Number;
		MsysInquire(MilSystem[i], M_NUMBER, &Number);
		switch (Number)
		{
		case M_DEV0: MosPrintf(MIL_TEXT("Camera %d (%s): device information in the first board.\n"), i, IsRemote ? MIL_TEXT("remote") : MIL_TEXT("local")); break;
		case M_DEV1: MosPrintf(MIL_TEXT("Camera %d (%s): device information in the second board.\n"), i, IsRemote ? MIL_TEXT("remote") : MIL_TEXT("local")); break;
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

	// ---- Image buffers, selection and live grab ------------------------------
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
