#include <mil.h>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <queue>
#include <thread>
#include <atomic>
#include <shared_mutex>
#include <windows.h>

#include <time.h>

#define N_FRAME 10
#define CAPTURE_THREAD 10
#define SAVE_THREAD 4
#define MOVE_THREAD 12

using namespace std;
using std::thread;

MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr);
void CaptureSingleImage(MIL_ID MilDigitizer, MIL_ID* MilBuffers, void* HookDataPtr, atomic <bool>& wait, atomic <bool>& stop);
void saveQueue(atomic <bool>& stop);
void pushSaveQueue(MIL_ID GrabImage, MIL_ID system_id, MIL_INT system_location, int save_idx, int digit_idx);
void moveQueue(MIL_ID host_app);
void pushMoveQueue(MIL_ID system_id, MIL_STRING file_name);

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

typedef struct
{
	MIL_ID GrabImage;
	MIL_ID system_id;
	MIL_INT system_location;
	int save_idx;
	int digit_idx;
} SaveQueueStruct;

typedef struct
{
	MIL_ID system_id;
	MIL_STRING file_name;
} MoveQueueStruct;

typedef struct
{
	MIL_INT ProcessedImageCount;
	MIL_INT system_location;
	MIL_ID system_id;
	int save_idx;
	int digit_idx;
	MIL_INT64 Height;
	MIL_INT64 Width;
} HookDataStruct;

queue<MoveQueueStruct> move_queue;
queue<SaveQueueStruct> save_queue;
shared_mutex SaveMutex;
shared_mutex MoveMutex;

int MosMain() {
	MIL_ID MilApplication;
	MIL_ID MilSystem[10];
	MIL_ID MilDisplay[10];
	MIL_ID MilImage[10];
	MIL_ID MilDigitizer[10];
	HookDataStruct UserHookData[10];
	MIL_INT ProcessFrameCount[10];
	MIL_INT ProcessFrameMissedCount[10];
	MIL_DOUBLE ProcessFrameRate[10];
	atomic<bool> stop = false;
	atomic<bool> wait = true;
	atomic<bool> capture = false;

	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);
	MappControlMp(M_DEFAULT, M_MP_USE, M_DEFAULT, M_ENABLE, M_NULL);

	MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV0, M_DEFAULT, &MilSystem[0]);
	MsysControl(MilSystem[0], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[0], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[0]);
	MdigAlloc(MilSystem[0], M_DEV0, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[0]);

	MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV1, M_DEFAULT, &MilSystem[1]);
	MsysControl(MilSystem[1], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[1], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[1]);
	MdigAlloc(MilSystem[1], M_DEV0, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[1]);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV0, M_DEFAULT, &MilSystem[2]);
	MsysControl(MilSystem[2], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[2], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[2]);
	MdigAlloc(MilSystem[2], M_DEV0, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[2]);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV1, M_DEFAULT, &MilSystem[3]);
	MsysControl(MilSystem[3], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[3], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[3]);
	MdigAlloc(MilSystem[3], M_DEV0, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[3]);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV1, M_DEFAULT, &MilSystem[4]);
	MsysControl(MilSystem[4], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[4], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[4]);
	MdigAlloc(MilSystem[4], M_DEV0, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[4]);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV0, M_DEFAULT, &MilSystem[5]);
	MsysControl(MilSystem[5], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[5], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[5]);
	MdigAlloc(MilSystem[5], M_DEV0, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[5]);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV0, M_DEFAULT, &MilSystem[6]);
	MsysControl(MilSystem[6], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[6], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[6]);
	MdigAlloc(MilSystem[6], M_DEV0, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[6]);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV1, M_DEFAULT, &MilSystem[7]);
	MsysControl(MilSystem[7], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[7], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[7]);
	MdigAlloc(MilSystem[7], M_DEV0, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[7]);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV0, M_DEFAULT, &MilSystem[8]);
	MsysControl(MilSystem[8], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[8], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[8]);
	MdigAlloc(MilSystem[8], M_DEV0, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[8]);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV1, M_DEFAULT, &MilSystem[9]);
	MsysControl(MilSystem[9], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[9], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[9]);
	MdigAlloc(MilSystem[9], M_DEV0, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[9]);


	MIL_INT64 Height = 5120;
	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControlFeature(MilDigitizer[4], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControlFeature(MilDigitizer[5], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControlFeature(MilDigitizer[6], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControlFeature(MilDigitizer[7], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControlFeature(MilDigitizer[8], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControlFeature(MilDigitizer[9], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControl(MilDigitizer[0], M_SOURCE_SIZE_Y, Height);
	MdigControl(MilDigitizer[1], M_SOURCE_SIZE_Y, Height);
	MdigControl(MilDigitizer[2], M_SOURCE_SIZE_Y, Height);
	MdigControl(MilDigitizer[3], M_SOURCE_SIZE_Y, Height);
	MdigControl(MilDigitizer[4], M_SOURCE_SIZE_Y, Height);
	MdigControl(MilDigitizer[5], M_SOURCE_SIZE_Y, Height);
	MdigControl(MilDigitizer[6], M_SOURCE_SIZE_Y, Height);
	MdigControl(MilDigitizer[7], M_SOURCE_SIZE_Y, Height);
	MdigControl(MilDigitizer[8], M_SOURCE_SIZE_Y, Height);
	MdigControl(MilDigitizer[9], M_SOURCE_SIZE_Y, Height);

	MIL_INT64 Width = 5120;
	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControlFeature(MilDigitizer[4], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControlFeature(MilDigitizer[5], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControlFeature(MilDigitizer[6], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControlFeature(MilDigitizer[7], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControlFeature(MilDigitizer[8], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControlFeature(MilDigitizer[9], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControl(MilDigitizer[0], M_SOURCE_SIZE_X, Width);
	MdigControl(MilDigitizer[1], M_SOURCE_SIZE_X, Width);
	MdigControl(MilDigitizer[2], M_SOURCE_SIZE_X, Width);
	MdigControl(MilDigitizer[3], M_SOURCE_SIZE_X, Width);
	MdigControl(MilDigitizer[4], M_SOURCE_SIZE_X, Width);
	MdigControl(MilDigitizer[5], M_SOURCE_SIZE_X, Width);
	MdigControl(MilDigitizer[6], M_SOURCE_SIZE_X, Width);
	MdigControl(MilDigitizer[7], M_SOURCE_SIZE_X, Width);
	MdigControl(MilDigitizer[8], M_SOURCE_SIZE_X, Width);
	MdigControl(MilDigitizer[9], M_SOURCE_SIZE_X, Width);

	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));
	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));
	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));
	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));
	MdigControlFeature(MilDigitizer[4], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));
	MdigControlFeature(MilDigitizer[5], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));
	MdigControlFeature(MilDigitizer[6], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));
	MdigControlFeature(MilDigitizer[7], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));
	MdigControlFeature(MilDigitizer[8], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));
	MdigControlFeature(MilDigitizer[9], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));

	MIL_DOUBLE ExposureTime = 999500;
	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);
	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);
	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);
	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);
	MdigControlFeature(MilDigitizer[4], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);
	MdigControlFeature(MilDigitizer[5], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);
	MdigControlFeature(MilDigitizer[6], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);
	MdigControlFeature(MilDigitizer[7], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);
	MdigControlFeature(MilDigitizer[8], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);
	MdigControlFeature(MilDigitizer[9], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);

	MIL_DOUBLE AcquisitionFrameRate = 1;
	MdigControlFeature(MilDigitizer[0], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);
	MdigControlFeature(MilDigitizer[1], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);
	MdigControlFeature(MilDigitizer[2], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);
	MdigControlFeature(MilDigitizer[3], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);
	MdigControlFeature(MilDigitizer[4], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);
	MdigControlFeature(MilDigitizer[5], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);
	MdigControlFeature(MilDigitizer[6], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);
	MdigControlFeature(MilDigitizer[7], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);
	MdigControlFeature(MilDigitizer[8], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);
	MdigControlFeature(MilDigitizer[9], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);


	MbufAlloc2d(MilSystem[0], (MIL_INT)(MdigInquire(MilDigitizer[0], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[0], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[0]);
	MbufAlloc2d(MilSystem[1], (MIL_INT)(MdigInquire(MilDigitizer[1], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[1], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[1]);
	MbufAlloc2d(MilSystem[2], (MIL_INT)(MdigInquire(MilDigitizer[2], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[2], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[2]);
	MbufAlloc2d(MilSystem[3], (MIL_INT)(MdigInquire(MilDigitizer[3], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[3], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[3]);
	MbufAlloc2d(MilSystem[4], (MIL_INT)(MdigInquire(MilDigitizer[4], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[4], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[4]);
	MbufAlloc2d(MilSystem[5], (MIL_INT)(MdigInquire(MilDigitizer[5], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[5], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[5]);
	MbufAlloc2d(MilSystem[6], (MIL_INT)(MdigInquire(MilDigitizer[6], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[6], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[6]);
	MbufAlloc2d(MilSystem[7], (MIL_INT)(MdigInquire(MilDigitizer[7], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[7], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[7]);
	MbufAlloc2d(MilSystem[8], (MIL_INT)(MdigInquire(MilDigitizer[8], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[8], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[8]);
	MbufAlloc2d(MilSystem[9], (MIL_INT)(MdigInquire(MilDigitizer[9], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[9], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[9]);

	MbufClear(MilImage[0], 0L);
	MbufClear(MilImage[1], 0L);
	MbufClear(MilImage[2], 0L);
	MbufClear(MilImage[3], 0L);
	MbufClear(MilImage[4], 0L);
	MbufClear(MilImage[5], 0L);
	MbufClear(MilImage[6], 0L);
	MbufClear(MilImage[7], 0L);
	MbufClear(MilImage[8], 0L);
	MbufClear(MilImage[9], 0L);


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

	MdigControlFeature(MilDigitizer[4], M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
	MdigControlFeature(MilDigitizer[4], M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
	MdigControlFeature(MilDigitizer[4], M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
	MdigControlFeature(MilDigitizer[4], M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, MIL_TEXT("RisingEdge"));

	MdigControlFeature(MilDigitizer[5], M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
	MdigControlFeature(MilDigitizer[5], M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
	MdigControlFeature(MilDigitizer[5], M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
	MdigControlFeature(MilDigitizer[5], M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, MIL_TEXT("RisingEdge"));

	MdigControlFeature(MilDigitizer[6], M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
	MdigControlFeature(MilDigitizer[6], M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
	MdigControlFeature(MilDigitizer[6], M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
	MdigControlFeature(MilDigitizer[6], M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, MIL_TEXT("RisingEdge"));

	MdigControlFeature(MilDigitizer[7], M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
	MdigControlFeature(MilDigitizer[7], M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
	MdigControlFeature(MilDigitizer[7], M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
	MdigControlFeature(MilDigitizer[7], M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, MIL_TEXT("RisingEdge"));

	MdigControlFeature(MilDigitizer[8], M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
	MdigControlFeature(MilDigitizer[8], M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
	MdigControlFeature(MilDigitizer[8], M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
	MdigControlFeature(MilDigitizer[8], M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, MIL_TEXT("RisingEdge"));

	MdigControlFeature(MilDigitizer[9], M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
	MdigControlFeature(MilDigitizer[9], M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
	MdigControlFeature(MilDigitizer[9], M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
	MdigControlFeature(MilDigitizer[9], M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, MIL_TEXT("RisingEdge"));

	MdigControl(MilDigitizer[0], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[0], M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
	MdigControl(MilDigitizer[0], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	

	MdigControl(MilDigitizer[1], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[1], M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
	MdigControl(MilDigitizer[1], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	

	MdigControl(MilDigitizer[2], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[2], M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
	MdigControl(MilDigitizer[2], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	

	MdigControl(MilDigitizer[3], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[3], M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
	MdigControl(MilDigitizer[3], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	
	
	MdigControl(MilDigitizer[4], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[4], M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
	MdigControl(MilDigitizer[4], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	
	
	MdigControl(MilDigitizer[5], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[5], M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
	MdigControl(MilDigitizer[5], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	
	
	MdigControl(MilDigitizer[6], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[6], M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
	MdigControl(MilDigitizer[6], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	
	
	MdigControl(MilDigitizer[7], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[7], M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
	MdigControl(MilDigitizer[7], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	
	
	MdigControl(MilDigitizer[8], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[8], M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
	MdigControl(MilDigitizer[8], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	
	
	MdigControl(MilDigitizer[9], M_IO_MODE + M_AUX_IO4, M_INPUT);
	MdigControl(MilDigitizer[9], M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
	MdigControl(MilDigitizer[9], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	

	MIL_ID MilImageBuf[10][N_FRAME];
	for (int n = 0; n < N_FRAME; n++)
	{
		MbufAlloc2d(MilSystem[0], (MIL_INT)(MdigInquire(MilDigitizer[0], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[0], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[0][n]);
		MbufAlloc2d(MilSystem[1], (MIL_INT)(MdigInquire(MilDigitizer[1], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[1], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[1][n]);
		MbufAlloc2d(MilSystem[2], (MIL_INT)(MdigInquire(MilDigitizer[2], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[2], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[2][n]);
		MbufAlloc2d(MilSystem[3], (MIL_INT)(MdigInquire(MilDigitizer[3], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[3], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[3][n]);
		MbufAlloc2d(MilSystem[4], (MIL_INT)(MdigInquire(MilDigitizer[4], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[4], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[4][n]);
		MbufAlloc2d(MilSystem[5], (MIL_INT)(MdigInquire(MilDigitizer[5], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[5], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[5][n]);
		MbufAlloc2d(MilSystem[6], (MIL_INT)(MdigInquire(MilDigitizer[6], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[6], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[6][n]);
		MbufAlloc2d(MilSystem[7], (MIL_INT)(MdigInquire(MilDigitizer[7], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[7], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[7][n]);
		MbufAlloc2d(MilSystem[8], (MIL_INT)(MdigInquire(MilDigitizer[8], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[8], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[8][n]);
		MbufAlloc2d(MilSystem[9], (MIL_INT)(MdigInquire(MilDigitizer[9], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[9], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[9][n]);

		if (!MilImageBuf[0][n] | !MilImageBuf[1][n] | !MilImageBuf[2][n] | !MilImageBuf[3][n] | !MilImageBuf[4][n] | !MilImageBuf[5][n] | !MilImageBuf[6][n] | !MilImageBuf[7][n] | !MilImageBuf[8][n] | !MilImageBuf[9][n]) {
			cout << "Allocation Failed\n";
			exit(-1);
		}

		MbufClear(MilImageBuf[0][n], 0L);
		MbufClear(MilImageBuf[1][n], 0L);
		MbufClear(MilImageBuf[2][n], 0L);
		MbufClear(MilImageBuf[3][n], 0L);
		MbufClear(MilImageBuf[4][n], 0L);
		MbufClear(MilImageBuf[5][n], 0L);
		MbufClear(MilImageBuf[6][n], 0L);
		MbufClear(MilImageBuf[7][n], 0L);
		MbufClear(MilImageBuf[8][n], 0L);
		MbufClear(MilImageBuf[9][n], 0L);
	}


	for (int n = 0; n < 10; n++) {
		UserHookData[n].ProcessedImageCount = 0;
		UserHookData[n].system_location = MsysInquire(MilSystem[n], M_DISTRIBUTED_MIL_TYPE, M_NULL);
		UserHookData[n].system_id = MsysInquire(MilSystem[n], M_OWNER_APPLICATION, M_NULL);
		UserHookData[n].save_idx = 0;
		UserHookData[n].digit_idx = n + 1;
		UserHookData[n].Height = Height;
		UserHookData[n].Width = Width;
	}


	MdigControl(MilDigitizer[0], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
	MdigControl(MilDigitizer[1], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
	MdigControl(MilDigitizer[2], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
	MdigControl(MilDigitizer[3], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
	MdigControl(MilDigitizer[4], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
	MdigControl(MilDigitizer[5], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
	MdigControl(MilDigitizer[6], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
	MdigControl(MilDigitizer[7], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
	MdigControl(MilDigitizer[8], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
	MdigControl(MilDigitizer[9], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);


	MosPrintf(MIL_TEXT("Press [Enter] to Capture One Image Each Camera...\n"));
	MosPrintf(MIL_TEXT("Press [Q] to Stop Capture Process...\n"));

	MIL_INT count = 0;

	thread capture_t[CAPTURE_THREAD];
	for (int n = 0; n < CAPTURE_THREAD; n++)
	{
		capture_t[n] = thread(CaptureSingleImage, MilDigitizer[n], MilImageBuf[n], &UserHookData[n], ref(wait), ref(stop));
	}

	thread save_t[SAVE_THREAD];
	for (int n = 0; n < SAVE_THREAD; n++)
	{
		save_t[n] = thread(saveQueue, ref(stop));
	}

	MIL_INT KeyPressed;
	boolean capturing = true;
	while (capturing)
	{
		MosPrintf(MIL_TEXT("Press Key you want...\n"));
		KeyPressed = MosGetch();
		
		if (KeyPressed == 13) {
			MosPrintf(MIL_TEXT("[Enter] Key Pressed\n"));
			wait = false;
			MosSleep(1000);
			wait = true;
			count++;
			MosPrintf(MIL_TEXT("%d Image Captured!\n"), (int)count);
		}

		if (KeyPressed == 81 || KeyPressed == 113) {
			MosPrintf(MIL_TEXT("[Q] Key Pressed\n"));
			capturing = false;
			stop = true;
		}

		MosSleep(1000);
	}

	for (int n = 0; n < CAPTURE_THREAD; n++)
	{
		capture_t[n].join();
	}

	for (int n = 0; n < SAVE_THREAD; n++)
	{
		save_t[n].join();
	}

	MosPrintf(MIL_TEXT("Gathering Images...\n"));

	clock_t start = clock();

	thread move_t[MOVE_THREAD];
	for (int n = 0; n < MOVE_THREAD; n++)
	{
		move_t[n] = thread(moveQueue, MilApplication);
	}

	for (int n = 0; n < MOVE_THREAD; n++)
	{
		move_t[n].join();
	}

	clock_t finish = clock();

	double duration = (double)(finish - start) / CLOCKS_PER_SEC;

	cout << duration << "seconds" << endl;


	MosPrintf(MIL_TEXT("Process Done!\n"));
	MosGetch();


	for (int n = 0; n < N_FRAME; n++)
	{
		MbufFree(MilImageBuf[0][n]);
		MbufFree(MilImageBuf[1][n]);
		MbufFree(MilImageBuf[2][n]);
		MbufFree(MilImageBuf[3][n]);
		MbufFree(MilImageBuf[4][n]);
		MbufFree(MilImageBuf[5][n]);
		MbufFree(MilImageBuf[6][n]);
		MbufFree(MilImageBuf[7][n]);
		MbufFree(MilImageBuf[8][n]);
		MbufFree(MilImageBuf[9][n]);
	}


	MbufFree(MilImage[0]); MbufFree(MilImage[1]); MbufFree(MilImage[2]); MbufFree(MilImage[3]); MbufFree(MilImage[4]); MbufFree(MilImage[5]); MbufFree(MilImage[6]); MbufFree(MilImage[7]); MbufFree(MilImage[8]); MbufFree(MilImage[9]);
	MdispFree(MilDisplay[0]); MdispFree(MilDisplay[1]); MdispFree(MilDisplay[2]); MdispFree(MilDisplay[3]); MdispFree(MilDisplay[4]); MdispFree(MilDisplay[5]); MdispFree(MilDisplay[6]); MdispFree(MilDisplay[7]); MdispFree(MilDisplay[8]); MdispFree(MilDisplay[9]);
	MdigFree(MilDigitizer[0]); MdigFree(MilDigitizer[1]); MdigFree(MilDigitizer[2]); MdigFree(MilDigitizer[3]); MdigFree(MilDigitizer[4]); MdigFree(MilDigitizer[5]); MdigFree(MilDigitizer[6]); MdigFree(MilDigitizer[7]); MdigFree(MilDigitizer[8]); MdigFree(MilDigitizer[9]);
	MsysFree(MilSystem[0]); MsysFree(MilSystem[1]); MsysFree(MilSystem[2]); MsysFree(MilSystem[3]); MsysFree(MilSystem[4]); MsysFree(MilSystem[5]); MsysFree(MilSystem[6]); MsysFree(MilSystem[7]); MsysFree(MilSystem[8]); MsysFree(MilSystem[9]);
	MappFree(MilApplication);

	return 0;
}

MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr)
{

	HookDataStruct* UserHookDataPtr = (HookDataStruct*)HookDataPtr;
	MIL_ID SaveBufferId, QueueId;
	UserHookDataPtr->ProcessedImageCount++;

	MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &SaveBufferId);

	MbufClone(SaveBufferId, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_IMAGE + M_HOST_MEMORY + M_OFF_BOARD, M_COPY_SOURCE_DATA, &QueueId);

	pushSaveQueue(QueueId, UserHookDataPtr->system_id, UserHookDataPtr->system_location, UserHookDataPtr->save_idx + (int)UserHookDataPtr->ProcessedImageCount, UserHookDataPtr->digit_idx);

	return 0;
}

void CaptureSingleImage(MIL_ID MilDigitizer, MIL_ID* MilBuffers, void* HookDataPtr, atomic <bool>& wait, atomic <bool>& stop)
{
	while (true)
	{
		if (wait) {
			if (stop) {
				break;
			}
			Sleep(500);
		}
		else {
			HookDataStruct* UserHookDataPtr = (HookDataStruct*)HookDataPtr;
			MIL_ID QueueId;
			MIL_INT buffer_num = UserHookDataPtr->ProcessedImageCount % N_FRAME;
			UserHookDataPtr->ProcessedImageCount++;

			MbufClear(MilBuffers[buffer_num], 0L);
			MdigGrab(MilDigitizer, MilBuffers[buffer_num]);
			MbufClone(MilBuffers[buffer_num], M_DEFAULT, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_IMAGE + M_HOST_MEMORY + M_OFF_BOARD, M_COPY_SOURCE_DATA, &QueueId);
			pushSaveQueue(QueueId, UserHookDataPtr->system_id, UserHookDataPtr->system_location, UserHookDataPtr->save_idx + (int)UserHookDataPtr->ProcessedImageCount, UserHookDataPtr->digit_idx);
		}
	}
}


void pushSaveQueue(MIL_ID GrabImage, MIL_ID system_id, MIL_INT system_location, int save_idx, int digit_idx)
{
	lock_guard lk(SaveMutex);

	SaveQueueStruct queue_entry = { GrabImage, system_id, system_location, save_idx, digit_idx };
	//queue_entry.GrabImage = GrabImage;
	//queue_entry.system_id = system_id;
	//queue_entry.system_location = system_location;
	//queue_entry.save_idx = save_idx;
	//queue_entry.digit_idx = digit_idx;

	save_queue.push(queue_entry);
}

void saveQueue(atomic <bool>& stop)
{
	while (true)
	{
		if (!save_queue.empty())
		{
			unique_lock lk(SaveMutex);

			SaveQueueStruct front_queue = save_queue.front();
			save_queue.pop();

			lk.unlock();


			stringstream s_idx;
			s_idx << setw(4) << setfill('0') << front_queue.save_idx;
			string save_idx = s_idx.str();
			wstring w_save_idx = wstring(save_idx.begin(), save_idx.end());

			stringstream d_idx;
			d_idx << setw(2) << setfill('0') << front_queue.digit_idx;
			string digit_idx = d_idx.str();
			wstring w_digit_idx = wstring(digit_idx.begin(), digit_idx.end());

			MIL_STRING_STREAM stream;
			MIL_STRING file_name;

			if (front_queue.system_location == M_DMIL_REMOTE)
			{
				stream << MIL_TEXT("remote:///") << MIL_TEXT("C:/output_image/") << (MIL_STRING)w_save_idx.c_str() << MIL_TEXT("_") << (MIL_STRING)w_digit_idx.c_str() << MIL_TEXT(".tiff");
				file_name = stream.str();

				MbufSave(file_name, front_queue.GrabImage);

				MbufFree(front_queue.GrabImage);

				MIL_STRING_STREAM QueueStream;
				QueueStream << MIL_TEXT("C:/output_image/") << (MIL_STRING)w_save_idx.c_str() << MIL_TEXT("_") << (MIL_STRING)w_digit_idx.c_str() << MIL_TEXT(".tiff");
				pushMoveQueue(front_queue.system_id, QueueStream.str());
			}
			else {
				stream << MIL_TEXT("C:/output_image/") << (MIL_STRING)w_save_idx.c_str() << MIL_TEXT("_") << (MIL_STRING)w_digit_idx.c_str() << MIL_TEXT(".tiff");
				file_name = stream.str();

				MbufSave(file_name, front_queue.GrabImage);

				MbufFree(front_queue.GrabImage);
			}
		}
		else {
			if (stop) {
				break;
			}

			//MosPrintf(MIL_TEXT("Wait for grabbing...\n"));
			Sleep(3000);
		}
	}
}

void pushMoveQueue(MIL_ID system_id, MIL_STRING file_name)
{
	lock_guard lk(MoveMutex);

	MoveQueueStruct queue_entry = { system_id, file_name };
	//queue_entry.system_id = system_id;
	//queue_entry.file_name = file_name;

	move_queue.push(queue_entry);
}

void moveQueue(MIL_ID host_app)
{
	while (!move_queue.empty())
	{
		unique_lock lk(MoveMutex);

		MoveQueueStruct front_queue = move_queue.front();
		move_queue.pop();

		lk.unlock();

		MappFileOperation(front_queue.system_id, front_queue.file_name, host_app, front_queue.file_name, M_FILE_MOVE, M_DEFAULT, M_NULL);
	}
}