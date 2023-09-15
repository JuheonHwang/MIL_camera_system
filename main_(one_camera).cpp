#include "mil.h"
#include <iostream>

using namespace std;

/*
int main() {
	MIL_ID MilApplication;
	MIL_ID MilSystem;
	MIL_ID MilDisplay;
	MIL_ID MilImage;
	MIL_ID MilDigitizer;

	MappAllocDefault(M_DEFAULT, &MilApplication, &MilSystem, &MilDisplay, &MilDigitizer, &MilImage);

	MIL_INT DigNumber;
	MdigInquire(MilDigitizer, M_NUMBER, &DigNumber);
	switch (DigNumber)
	{
	case M_DEV0: MosPrintf(MIL_TEXT("system number zero.\n")); break;
	case M_DEV1: MosPrintf(MIL_TEXT("system number one.\n")); break;
	case M_DEV2: MosPrintf(MIL_TEXT("system number two.\n")); break;
	case M_DEV3: MosPrintf(MIL_TEXT("system number three.\n")); break;
	}

	MappFreeDefault(MilApplication, MilSystem, MilDisplay, MilDigitizer, MilImage);
}*/

MIL_DOUBLE round_digit(MIL_DOUBLE num, int d)
{
	MIL_DOUBLE t = pow(10, d - 1);
	return floor(num * t) / t;
}


int MosMain() {
	MIL_ID MilApplication;
	MIL_ID MilSystem;
	MIL_ID MilDisplay;
	MIL_ID MilImage;
	MIL_ID MilDigitizer;


	//MappAlloc(M_DEFAULT, &MilApplication);
	//MsysAlloc(M_DEFAULT, M_SYSTEM_RAPIXOCXP, M_DEV0, M_DEFAULT, &MilSystem);
	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);
	MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV0, M_DEFAULT, &MilSystem);

	/*
	MIL_INT Number;
	MsysInquire(MilSystem, M_NUMBER, &Number);
	switch (Number)
	{
	case M_DEV0: MosPrintf(MIL_TEXT("system number zero.\n")); break;
	case M_DEV1: MosPrintf(MIL_TEXT("system number one.\n")); break;
	}
	*/


	MdispAlloc(MilSystem, M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay);

	//MdigAlloc(MilSystem, M_DEV3, MIL_TEXT("C:/Users/InsightSensor3/DCF.dcf"), M_DEFAULT, &MilDigitizer);
	//MdigAlloc(MilSystem, M_DEV3, MIL_TEXT("C:/Program Files/Matrox Imaging/Drivers/RapixoCXP/dcf/DefaultFrameScan.dcf"), M_DEFAULT, &MilDigitizer);
	//MdigAlloc(MilSystem, M_DEV3, MIL_TEXT("C:/Users/InsightSensor3/AppData/Local/Temp/DCF1.dcf"), M_DEFAULT, &MilDigitizer);
	MdigAlloc(MilSystem, M_DEV3, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer);

	
	MIL_INT64 Height = 5120;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControl(MilDigitizer, M_SOURCE_SIZE_Y, Height);

	MIL_INT64 Width = 5120;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControl(MilDigitizer, M_SOURCE_SIZE_X, Width);

	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));

	MIL_DOUBLE ExposureTime = 60000;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);

	MIL_DOUBLE AcquisitionFrameRate = round_digit(1000000 / ExposureTime, 2);
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);	



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


	//MdigAlloc(MilSystem, M_DEV3, MIL_TEXT("VC-25MX2-M150I1"), M_DEFAULT, &MilDigitizer);
	//MdigAlloc(MilSystem, M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer);

	//MbufAlloc2d(MilSystem, 5120, 5120, 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage);
	MbufAlloc2d(MilSystem, (MIL_INT) (MdigInquire(MilDigitizer, M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage);

	MbufClear(MilImage, 0L);
	//MgraArcFill(M_DEFAULT, MilImage, 256L, 240L, 100L, 100L, 0.0, 360.0);
	//MgraText(M_DEFAULT, MilImage, 238L, 234L, MIL_TEXT("MIL"));

	MdispSelect(MilDisplay, MilImage);

	MdigGrabContinuous(MilDigitizer, MilImage);
	//MdigGrab(MilDigitizer, MilImage);


	MosGetch();

	MdigHalt(MilDigitizer);

	MIL_STRING Format;
	MdigInquire(MilDigitizer, M_FORMAT, Format);
	MosPrintf(MIL_TEXT("Format: %s\n"), Format.c_str());

	MIL_INT ChannelNum;
	MdigInquire(MilDigitizer, M_CHANNEL_NUM, &ChannelNum);
	MosPrintf(MIL_TEXT("Channel Num: %d\n", ChannelNum));

	MIL_INT DigNumber;
	MdigInquire(MilDigitizer, M_NUMBER, &DigNumber);
	switch (DigNumber)
	{
	case M_DEV0: MosPrintf(MIL_TEXT("system number zero.\n")); break;
	case M_DEV1: MosPrintf(MIL_TEXT("system number one.\n")); break;
	case M_DEV2: MosPrintf(MIL_TEXT("system number two.\n")); break;
	case M_DEV3: MosPrintf(MIL_TEXT("system number three.\n")); break;
	}

	MIL_DOUBLE SelectedFrameRate;
	MdigInquire(MilDigitizer, M_SELECTED_FRAME_RATE, &SelectedFrameRate);
	MosPrintf(MIL_TEXT("Selected Frame Rate: %f\n", SelectedFrameRate));

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
	MosPrintf(MIL_TEXT("Size Band: %d\n", SizeBand));

	MIL_INT SizeBit;
	MdigInquire(MilDigitizer, M_SIZE_BIT, &SizeBit);
	MosPrintf(MIL_TEXT("Size Bit: %d\n", SizeBit));

	MIL_INT SizeX; MIL_INT SizeY;
	MdigInquire(MilDigitizer, M_SIZE_X, &SizeX);
	MdigInquire(MilDigitizer, M_SIZE_Y, &SizeY);
	MosPrintf(MIL_TEXT("Size X & Y: %d, %d\n", SizeX, SizeY));

	MIL_INT Type;
	MdigInquire(MilDigitizer, M_TYPE, &Type);
	MosPrintf(MIL_TEXT("Type: %d\n", Type));

	//MosPrintf(MIL_TEXT("A circle was drawn in the displayed image buffer.\n"));


	
	//MappTimerVoid(M_TIMER_WAIT);

	MbufFree(MilImage);
	MdispFree(MilDisplay);
	MdigFree(MilDigitizer);
	MsysFree(MilSystem);
	MappFree(MilApplication);
	

	return 0;
}