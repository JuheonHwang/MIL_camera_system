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

void capture(MIL_ID MilIm, MIL_ID MilDigit, atomic <bool>& grab)
{
	while (!grab) {

	}
	MdigGrab(MilDigit, MilIm);
}


int MosMain() {
	MIL_ID MilApplication;
	MIL_ID MilSystem[4];
	MIL_ID MilDisplay[4];
	MIL_ID MilImage[4];
	MIL_ID MilDigitizer[4];
	const bool show_cam_info = false;
	atomic<bool> grab = false;

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


	MbufAlloc2d(MilSystem[0], (MIL_INT)(MdigInquire(MilDigitizer[0], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[0], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[0]);
	MbufAlloc2d(MilSystem[1], (MIL_INT)(MdigInquire(MilDigitizer[1], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[1], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[1]);
	MbufAlloc2d(MilSystem[2], (MIL_INT)(MdigInquire(MilDigitizer[2], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[2], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[2]);
	MbufAlloc2d(MilSystem[3], (MIL_INT)(MdigInquire(MilDigitizer[3], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[3], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[3]);

	MbufClear(MilImage[0], 0L);
	MbufClear(MilImage[1], 0L);
	MbufClear(MilImage[2], 0L);
	MbufClear(MilImage[3], 0L);


	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, MIL_TEXT("RisingEdge"));

	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, MIL_TEXT("RisingEdge"));

	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, MIL_TEXT("RisingEdge"));

	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, MIL_TEXT("RisingEdge"));


	MdigControl(MilDigitizer[0], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[0], M_TL_TRIGGER_ACTIVATION + M_TL_TRIGGER, M_ANY_EDGE);
	MdigControl(MilDigitizer[0], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	MdigControl(MilDigitizer[0], M_GRAB_TRIGGER_SOURCE, M_AUX_IO4);
	MdigControl(MilDigitizer[0], M_GRAB_TRIGGER_ACTIVATION, M_EDGE_RISING);
	MdigControl(MilDigitizer[0], M_GRAB_TRIGGER_STATE, M_ENABLE);

	MdigControl(MilDigitizer[1], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[1], M_TL_TRIGGER_ACTIVATION + M_TL_TRIGGER, M_ANY_EDGE);
	MdigControl(MilDigitizer[1], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	MdigControl(MilDigitizer[1], M_GRAB_TRIGGER_SOURCE, M_AUX_IO4);
	MdigControl(MilDigitizer[1], M_GRAB_TRIGGER_ACTIVATION, M_EDGE_RISING);
	MdigControl(MilDigitizer[1], M_GRAB_TRIGGER_STATE, M_ENABLE);

	MdigControl(MilDigitizer[2], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[2], M_TL_TRIGGER_ACTIVATION + M_TL_TRIGGER, M_ANY_EDGE);
	MdigControl(MilDigitizer[2], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	MdigControl(MilDigitizer[2], M_GRAB_TRIGGER_SOURCE, M_AUX_IO4);
	MdigControl(MilDigitizer[2], M_GRAB_TRIGGER_ACTIVATION, M_EDGE_RISING);
	MdigControl(MilDigitizer[2], M_GRAB_TRIGGER_STATE, M_ENABLE);

	MdigControl(MilDigitizer[3], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[3], M_TL_TRIGGER_ACTIVATION + M_TL_TRIGGER, M_ANY_EDGE);
	MdigControl(MilDigitizer[3], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	MdigControl(MilDigitizer[3], M_GRAB_TRIGGER_SOURCE, M_AUX_IO4);
	MdigControl(MilDigitizer[3], M_GRAB_TRIGGER_ACTIVATION, M_EDGE_RISING);
	MdigControl(MilDigitizer[3], M_GRAB_TRIGGER_STATE, M_ENABLE);

	thread t1(capture, MilImage[0], MilDigitizer[0], ref(grab));
	thread t2(capture, MilImage[1], MilDigitizer[1], ref(grab));
	thread t3(capture, MilImage[2], MilDigitizer[2], ref(grab));
	thread t4(capture, MilImage[3], MilDigitizer[3], ref(grab));

	MosPrintf(MIL_TEXT("Press any key to capture start\n"));

	MosGetch();
	grab = true;

	t1.join();
	t2.join();
	t3.join();
	t4.join();

	Mat cvImage[4];
	cvImage[0].create(Size(Width, Height), CV_8U); MbufGet(MilImage[0], cvImage[0].data);
	cvImage[1].create(Size(Width, Height), CV_8U); MbufGet(MilImage[1], cvImage[1].data);
	cvImage[2].create(Size(Width, Height), CV_8U); MbufGet(MilImage[2], cvImage[2].data);
	cvImage[3].create(Size(Width, Height), CV_8U); MbufGet(MilImage[3], cvImage[3].data);

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
	imwrite(output_folder + "/" + idx_ + "_1.png", cvImage[0]);
	imwrite(output_folder + "/" + idx_ + "_2.png", cvImage[1]);
	imwrite(output_folder + "/" + idx_ + "_3.png", cvImage[2]);
	imwrite(output_folder + "/" + idx_ + "_4.png", cvImage[3]);

	MosPrintf(MIL_TEXT("Image Saved!\n"));

	t1.detach();
	t2.detach();
	t3.detach();
	t4.detach();


	MbufFree(MilImage[0]); MbufFree(MilImage[1]); MbufFree(MilImage[2]); MbufFree(MilImage[3]);
	MdispFree(MilDisplay[0]); MdispFree(MilDisplay[1]); MdispFree(MilDisplay[2]); MdispFree(MilDisplay[3]);
	MdigFree(MilDigitizer[0]); MdigFree(MilDigitizer[1]); MdigFree(MilDigitizer[2]); MdigFree(MilDigitizer[3]);
	MsysFree(MilSystem[0]); MsysFree(MilSystem[1]); MsysFree(MilSystem[2]); MsysFree(MilSystem[3]);
	MappFree(MilApplication);

	return 0;
}