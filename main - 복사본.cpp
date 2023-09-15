#include <mil.h>
#include <thread>
#include <iostream>
#include <atomic>
#include <opencv2/highgui.hpp>
#include <filesystem>
#include <sstream>
#include <iomanip>

using namespace cv;
using namespace std;
using std::thread;
namespace fs = std::filesystem;

vector<string> split(string input, char delimiter) {
	vector<string> answer;
	stringstream ss(input);
	string temp;

	while (getline(ss, temp, delimiter)) {
		answer.push_back(temp);
	}

	return answer;
}

MIL_DOUBLE round_digit(MIL_DOUBLE num, int d)
{
	MIL_DOUBLE t = pow(10, d - 1);
	return floor(num * t) / t;
}

void display_cam(MIL_ID MilSys, MIL_ID MilDisp, MIL_ID MilIm, MIL_ID MilDigit, const int cam_id, const bool show_camera_info, atomic <bool>& stop)
{
	switch (cam_id)
	{
	case 1:
		MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV0, M_DEFAULT, &MilSys);
		break;
	case 2:
		MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV1, M_DEFAULT, &MilSys);
		break;
	case 3:
		MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV0, M_DEFAULT, &MilSys);
		break;
	case 4:
		MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV1, M_DEFAULT, &MilSys);
		break;
	}
	MdispAlloc(MilSys, M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisp);
	MdigAlloc(MilSys, M_DEV3, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigit);

	MIL_INT64 Height = 5120;
	MdigControlFeature(MilDigit, M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControl(MilDigit, M_SOURCE_SIZE_Y, Height);

	MIL_INT64 Width = 5120;
	MdigControlFeature(MilDigit, M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControl(MilDigit, M_SOURCE_SIZE_X, Width);

	MdigControlFeature(MilDigit, M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));

	MIL_DOUBLE ExposureTime = 60000;
	MdigControlFeature(MilDigit, M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);

	MIL_DOUBLE AcquisitionFrameRate = round_digit(1000000 / ExposureTime, 2);
	MdigControlFeature(MilDigit, M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);

	if (show_camera_info)
	{
		MIL_INT Number;
		MsysInquire(MilSys, M_NUMBER, &Number);
		switch (Number)
		{
		case M_DEV0: MosPrintf(MIL_TEXT("Device Information in the first board.\n")); break;
		case M_DEV1: MosPrintf(MIL_TEXT("Device Information in the second board.\n")); break;
		}

		MIL_STRING DeviceVendorName;
		MdigInquireFeature(MilDigit, M_FEATURE_VALUE, MIL_TEXT("DeviceVendorName"), M_TYPE_STRING, DeviceVendorName);
		MosPrintf(MIL_TEXT("Device Vendor Name: %s\n"), DeviceVendorName.c_str());

		MIL_STRING DeviceModelName;
		MdigInquireFeature(MilDigit, M_FEATURE_VALUE, MIL_TEXT("DeviceModelName"), M_TYPE_STRING, DeviceModelName);
		MosPrintf(MIL_TEXT("Device Model Name: %s\n"), DeviceModelName.c_str());

		MIL_STRING DeviceSerialNumber;
		MdigInquireFeature(MilDigit, M_FEATURE_VALUE, MIL_TEXT("DeviceSerialNumber"), M_TYPE_STRING, DeviceSerialNumber);
		MosPrintf(MIL_TEXT("Device Serial Number: %s\n"), DeviceSerialNumber.c_str());

		MIL_STRING PixelFormat;
		MdigInquireFeature(MilDigit, M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, PixelFormat);
		MosPrintf(MIL_TEXT("Pixel Format: %s\n"), PixelFormat.c_str());

		MosPrintf(MIL_TEXT("\n"));
	}

	MbufAlloc2d(MilSys, (MIL_INT)(MdigInquire(MilDigit, M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigit, M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilIm);

	MbufClear(MilIm, 0L);

	MdigControlFeature(MilDigit, M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
	MdigControlFeature(MilDigit, M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
	MdigControlFeature(MilDigit, M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
	MdigControlFeature(MilDigit, M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, MIL_TEXT("RisingEdge"));

	MdigControl(MilDigit, M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigit, M_TL_TRIGGER_ACTIVATION + M_TL_TRIGGER, M_ANY_EDGE);
	MdigControl(MilDigit, M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	MdigControl(MilDigit, M_GRAB_TRIGGER_SOURCE, M_AUX_IO4);
	MdigControl(MilDigit, M_GRAB_TRIGGER_ACTIVATION, M_EDGE_RISING);
	MdigControl(MilDigit, M_GRAB_TRIGGER_STATE, M_ENABLE);

	if (stop)
	{
		MdigGrab(MilDigit, MilIm);

		Mat cvImage;
		cvImage.create(Size(Width, Height), CV_8U); MbufGet(MilIm, cvImage.data);

		string output_folder = "./image_output";
		if (fs::is_directory(output_folder) != 1)
		{
			fs::create_directory(output_folder);
		}

		fs::path lastFileName;
		for (const auto& entry : fs::directory_iterator(output_folder))
		{
			if (entry.path() > lastFileName)
			{
				lastFileName = entry.path();
			}
		}

		int idx;
		if (lastFileName.empty())
		{
			idx = 0;
		}
		else
		{
			string lastFileName_ = lastFileName.filename().string().c_str();
			vector<string> lastFileNameSplit = split(lastFileName_, '_');
			idx = stoi(lastFileNameSplit[0]);
		}
		stringstream s_idx;
		s_idx << setw(2) << setfill('0') << idx + 1;
		string idx_ = s_idx.str();

		switch (cam_id)
		{
		case 1:
			imwrite(output_folder + "/" + idx_ + "_1.png", cvImage);
			break;
		case 2:
			imwrite(output_folder + "/" + idx_ + "_2.png", cvImage);
			break;
		case 3:
			imwrite(output_folder + "/" + idx_ + "_3.png", cvImage);
			break;
		case 4:
			imwrite(output_folder + "/" + idx_ + "_4.png", cvImage);
			break;
		}

		MbufFree(MilIm);
		MdispFree(MilDisp);
		MdigFree(MilDigit);
		MsysFree(MilSys);
	}
}


int MosMain() {
	MIL_ID MilApplication;
	MIL_ID MilSystem[4];
	MIL_ID MilDisplay[4];
	MIL_ID MilImage[4];
	MIL_ID MilDigitizer[4];
	const bool show_cam_info = false;
	atomic<bool> stop = false;

	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	thread t1(display_cam, MilSystem[0], MilDisplay[0], MilImage[0], MilDigitizer[0], 1, show_cam_info, ref(stop));
	thread t2(display_cam, MilSystem[1], MilDisplay[1], MilImage[1], MilDigitizer[1], 2, show_cam_info, ref(stop));
	thread t3(display_cam, MilSystem[2], MilDisplay[2], MilImage[2], MilDigitizer[2], 3, show_cam_info, ref(stop));
	thread t4(display_cam, MilSystem[3], MilDisplay[3], MilImage[3], MilDigitizer[3], 4, show_cam_info, ref(stop));

	MosPrintf(MIL_TEXT("Press any key to capture start\n"));

	MosGetch();
	stop = true;

	t1.detach();
	t2.detach();
	t3.detach();
	t4.detach();

	MosPrintf(MIL_TEXT("Image Saved!\n"));

	return 0;
}