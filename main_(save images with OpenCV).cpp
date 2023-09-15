#include <mil.h>
#include <opencv2/highgui.hpp>
#include <iostream>

using namespace cv;
using namespace std;


MIL_DOUBLE round_digit(MIL_DOUBLE num, int d)
{
	MIL_DOUBLE t = pow(10, d - 1);
	return floor(num * t) / t;
}


int MosMain() {
	MIL_ID MilApplication;
	MIL_ID MilSystem[4];
	MIL_ID MilDisplay[4];
	MIL_ID MilImage[4];
	MIL_ID MilDigitizer[4];


	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV0, M_DEFAULT, &MilSystem[0]);
	MdispAlloc(MilSystem[0], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[0]);
	MdigAlloc(MilSystem[0], M_DEV3, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[0]);

	MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV1, M_DEFAULT, &MilSystem[1]);
	MdispAlloc(MilSystem[1], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[1]);
	MdigAlloc(MilSystem[1], M_DEV3, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[1]);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV0, M_DEFAULT, &MilSystem[2]);
	MdispAlloc(MilSystem[2], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[2]);
	MdigAlloc(MilSystem[2], M_DEV3, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[2]);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV1, M_DEFAULT, &MilSystem[3]);
	MdispAlloc(MilSystem[3], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[3]);
	MdigAlloc(MilSystem[3], M_DEV3, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[3]);

	
	MIL_INT64 Height = 5120;
	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControl(MilDigitizer[0], M_SOURCE_SIZE_Y, Height);
	MdigControl(MilDigitizer[1], M_SOURCE_SIZE_Y, Height);
	MdigControl(MilDigitizer[2], M_SOURCE_SIZE_Y, Height);
	MdigControl(MilDigitizer[3], M_SOURCE_SIZE_Y, Height);

	MIL_INT64 Width = 5120;
	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControl(MilDigitizer[0], M_SOURCE_SIZE_X, Width);
	MdigControl(MilDigitizer[1], M_SOURCE_SIZE_X, Width);
	MdigControl(MilDigitizer[2], M_SOURCE_SIZE_X, Width);
	MdigControl(MilDigitizer[3], M_SOURCE_SIZE_X, Width);

	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));
	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));
	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));
	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));

	MIL_DOUBLE ExposureTime = 60000;
	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);
	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);
	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);
	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);

	MIL_DOUBLE AcquisitionFrameRate = round_digit(1000000 / ExposureTime, 2);
	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);
	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);
	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);
	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);



	MIL_INT Number;
	MsysInquire(MilSystem[0], M_NUMBER, &Number);
	switch (Number)
	{
	case M_DEV0: MosPrintf(MIL_TEXT("Device Information in the first board.\n")); break;
	case M_DEV1: MosPrintf(MIL_TEXT("Device Information in the second board.\n")); break;
	}

	MIL_STRING DeviceVendorName;
	MdigInquireFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("DeviceVendorName"), M_TYPE_STRING, DeviceVendorName);
	MosPrintf(MIL_TEXT("Device Vendor Name: %s\n"), DeviceVendorName.c_str());

	MIL_STRING DeviceModelName;
	MdigInquireFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("DeviceModelName"), M_TYPE_STRING, DeviceModelName);
	MosPrintf(MIL_TEXT("Device Model Name: %s\n"), DeviceModelName.c_str());

	MIL_STRING DeviceSerialNumber;
	MdigInquireFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("DeviceSerialNumber"), M_TYPE_STRING, DeviceSerialNumber);
	MosPrintf(MIL_TEXT("Device Serial Number: %s\n"), DeviceSerialNumber.c_str());

	MIL_STRING PixelFormat;
	MdigInquireFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, PixelFormat);
	MosPrintf(MIL_TEXT("Pixel Format: %s\n"), PixelFormat.c_str());

	MosPrintf(MIL_TEXT("\n"));

	MsysInquire(MilSystem[1], M_NUMBER, &Number);
	switch (Number)
	{
	case M_DEV0: MosPrintf(MIL_TEXT("Device Information in the first board.\n")); break;
	case M_DEV1: MosPrintf(MIL_TEXT("Device Information in the second board.\n")); break;
	}

	MdigInquireFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("DeviceVendorName"), M_TYPE_STRING, DeviceVendorName);
	MosPrintf(MIL_TEXT("Device Vendor Name: %s\n"), DeviceVendorName.c_str());

	MdigInquireFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("DeviceModelName"), M_TYPE_STRING, DeviceModelName);
	MosPrintf(MIL_TEXT("Device Model Name: %s\n"), DeviceModelName.c_str());

	MdigInquireFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("DeviceSerialNumber"), M_TYPE_STRING, DeviceSerialNumber);
	MosPrintf(MIL_TEXT("Device Serial Number: %s\n"), DeviceSerialNumber.c_str());

	MdigInquireFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, PixelFormat);
	MosPrintf(MIL_TEXT("Pixel Format: %s\n"), PixelFormat.c_str());

	MosPrintf(MIL_TEXT("\n"));

	MsysInquire(MilSystem[2], M_NUMBER, &Number);
	switch (Number)
	{
	case M_DEV0: MosPrintf(MIL_TEXT("Device Information in the first board.\n")); break;
	case M_DEV1: MosPrintf(MIL_TEXT("Device Information in the second board.\n")); break;
	}

	MdigInquireFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("DeviceVendorName"), M_TYPE_STRING, DeviceVendorName);
	MosPrintf(MIL_TEXT("Device Vendor Name: %s\n"), DeviceVendorName.c_str());

	MdigInquireFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("DeviceModelName"), M_TYPE_STRING, DeviceModelName);
	MosPrintf(MIL_TEXT("Device Model Name: %s\n"), DeviceModelName.c_str());

	MdigInquireFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("DeviceSerialNumber"), M_TYPE_STRING, DeviceSerialNumber);
	MosPrintf(MIL_TEXT("Device Serial Number: %s\n"), DeviceSerialNumber.c_str());

	MdigInquireFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, PixelFormat);
	MosPrintf(MIL_TEXT("Pixel Format: %s\n"), PixelFormat.c_str());

	MosPrintf(MIL_TEXT("\n"));

	MsysInquire(MilSystem[3], M_NUMBER, &Number);
	switch (Number)
	{
	case M_DEV0: MosPrintf(MIL_TEXT("Device Information in the first board.\n")); break;
	case M_DEV1: MosPrintf(MIL_TEXT("Device Information in the second board.\n")); break;
	}

	MdigInquireFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("DeviceVendorName"), M_TYPE_STRING, DeviceVendorName);
	MosPrintf(MIL_TEXT("Device Vendor Name: %s\n"), DeviceVendorName.c_str());

	MdigInquireFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("DeviceModelName"), M_TYPE_STRING, DeviceModelName);
	MosPrintf(MIL_TEXT("Device Model Name: %s\n"), DeviceModelName.c_str());

	MdigInquireFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("DeviceSerialNumber"), M_TYPE_STRING, DeviceSerialNumber);
	MosPrintf(MIL_TEXT("Device Serial Number: %s\n"), DeviceSerialNumber.c_str());

	MdigInquireFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, PixelFormat);
	MosPrintf(MIL_TEXT("Pixel Format: %s\n"), PixelFormat.c_str());

	MosPrintf(MIL_TEXT("\n"));
	MosPrintf(MIL_TEXT("\n"));


	//MbufAlloc2d(MilSystem, 5120, 5120, 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage);
	MbufAlloc2d(MilSystem[0], (MIL_INT)(MdigInquire(MilDigitizer[0], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[0], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[0]);
	MbufAlloc2d(MilSystem[1], (MIL_INT)(MdigInquire(MilDigitizer[1], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[1], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[1]);
	MbufAlloc2d(MilSystem[2], (MIL_INT)(MdigInquire(MilDigitizer[2], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[2], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[2]);
	MbufAlloc2d(MilSystem[3], (MIL_INT)(MdigInquire(MilDigitizer[3], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[3], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[3]);

	MbufClear(MilImage[0], 0L);
	MbufClear(MilImage[1], 0L);
	MbufClear(MilImage[2], 0L);
	MbufClear(MilImage[3], 0L);

	//MdispSelect(MilDisplay[0], MilImage[0]);
	//MdispSelect(MilDisplay[1], MilImage[1]);
	//MdispSelect(MilDisplay[2], MilImage[2]);
	//MdispSelect(MilDisplay[3], MilImage[3]);

	MdigGrab(MilDigitizer[0], MilImage[0]);
	MdigGrab(MilDigitizer[1], MilImage[1]);
	MdigGrab(MilDigitizer[2], MilImage[2]);
	MdigGrab(MilDigitizer[3], MilImage[3]);

	//MosGetch();

	Mat cvImage[4];
	cvImage[0].create(Size(Width, Height), CV_8U); MbufGet(MilImage[0], cvImage[0].data);
	cvImage[1].create(Size(Width, Height), CV_8U); MbufGet(MilImage[1], cvImage[1].data);
	cvImage[2].create(Size(Width, Height), CV_8U); MbufGet(MilImage[2], cvImage[2].data);
	cvImage[3].create(Size(Width, Height), CV_8U); MbufGet(MilImage[3], cvImage[3].data);

	imwrite("155_DEV0.bmp", cvImage[0]);
	imwrite("155_DEV1.bmp", cvImage[1]);
	imwrite("154_DEV0.bmp", cvImage[2]);
	imwrite("154_DEV1.bmp", cvImage[3]);

	MosPrintf(MIL_TEXT("Image Saved!\n"));


	MbufFree(MilImage[0]); MbufFree(MilImage[1]); MbufFree(MilImage[2]); MbufFree(MilImage[3]);
	MdispFree(MilDisplay[0]); MdispFree(MilDisplay[1]); MdispFree(MilDisplay[2]); MdispFree(MilDisplay[3]);
	MdigFree(MilDigitizer[0]); MdigFree(MilDigitizer[1]); MdigFree(MilDigitizer[2]); MdigFree(MilDigitizer[3]);
	MsysFree(MilSystem[0]); MsysFree(MilSystem[1]); MsysFree(MilSystem[2]); MsysFree(MilSystem[3]);
	MappFree(MilApplication);
	

	return 0;
}