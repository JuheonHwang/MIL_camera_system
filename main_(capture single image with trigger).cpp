#include <mil.h>
#include <opencv2/highgui.hpp>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace cv;
using namespace std;
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


int MosMain() {
	MIL_ID MilApplication;
	MIL_ID MilSystem;
	MIL_ID MilDisplay;
	MIL_ID MilImage;
	MIL_ID MilDigitizer;


	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV0, M_DEFAULT, &MilSystem);
	MdispAlloc(MilSystem, M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay);
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


	MbufAlloc2d(MilSystem, (MIL_INT)(MdigInquire(MilDigitizer, M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage);

	MbufClear(MilImage, 0L);

	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("Off"));

	MdigControl(MilDigitizer, M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer, M_GRAB_TRIGGER_SOURCE, M_AUX_IO4);
	MdigControl(MilDigitizer, M_GRAB_TRIGGER_ACTIVATION, M_EDGE_RISING);
	MdigControl(MilDigitizer, M_GRAB_TRIGGER_STATE, M_ENABLE);

	MosPrintf(MIL_TEXT("Device waits for signal...\n"));
	
	MdigGrab(MilDigitizer, MilImage);

	MosPrintf(MIL_TEXT("Device grabs the image...\n"));



	Mat cvImage;
	cvImage.create(Size(Width, Height), CV_8U); MbufGet(MilImage, cvImage.data);

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
	imwrite(output_folder + "/" + idx_ + "_1.png", cvImage);
	MosPrintf(MIL_TEXT("Image Saved!\n"));


	MbufFree(MilImage);
	MdispFree(MilDisplay);
	MdigFree(MilDigitizer);
	MsysFree(MilSystem);
	MappFree(MilApplication);


	return 0;
}