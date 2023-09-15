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

#define N_FRAME 10

using namespace std;
using std::thread;

MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr);

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
	MIL_ID system_id;
	MIL_STRING file_name;
} QueueStruct;

typedef struct
{
	MIL_INT ProcessedImageCount;
	MIL_ID system_id;
	MIL_INT remote_flag;
	int save_idx;
	int digit_idx;
	MIL_INT64 Height;
	MIL_INT64 Width;
} HookDataStruct;

queue<QueueStruct> move_queue;
shared_mutex sharedMutex;

void pushQueue(MIL_ID system_id, MIL_STRING file_name)
{
	lock_guard lk(sharedMutex);

	QueueStruct queue_entry = { system_id, file_name };
	//queue_entry.system_id = system_id;
	//queue_entry.file_name = file_name;

	move_queue.push(queue_entry);
}

void load2hostQueue(MIL_ID host_app, atomic <bool>& stop)
{
	while (true)
	{
		if (!move_queue.empty())
		{
			lock_guard lk(sharedMutex);

			QueueStruct front_queue = move_queue.front();
			move_queue.pop();

			MappFileOperation(front_queue.system_id, front_queue.file_name, host_app, front_queue.file_name, M_FILE_MOVE, M_DEFAULT, M_NULL);
		}
		else {
			if (stop) {
				break;
			}

			MosPrintf(MIL_TEXT("Wait for saving...\n"));
			Sleep(500);
		}
	}
}

int MosMain() {
	MIL_ID MilApplication;
	MIL_ID MilSystem[4];
	MIL_ID MilDisplay[4];
	MIL_ID MilImage[4];
	MIL_ID MilDigitizer[4];
	HookDataStruct UserHookData[4];
	MIL_INT ProcessFrameCount[4];
	MIL_INT ProcessFrameMissedCount[4];
	MIL_DOUBLE ProcessFrameRate[4];
	atomic<bool> stop = false;

	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV0, M_DEFAULT, &MilSystem[0]);
	MsysControl(MilSystem[0], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[0], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[0]);
	MdigAlloc(MilSystem[0], M_DEV3, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[0]);

	MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV1, M_DEFAULT, &MilSystem[1]);
	MsysControl(MilSystem[1], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[1], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[1]);
	MdigAlloc(MilSystem[1], M_DEV3, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[1]);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV0, M_DEFAULT, &MilSystem[2]);
	MsysControl(MilSystem[2], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[2], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[2]);
	MdigAlloc(MilSystem[2], M_DEV3, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[2]);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV1, M_DEFAULT, &MilSystem[3]);
	MsysControl(MilSystem[3], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
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


	MIL_ID MilImageBuf[4][N_FRAME];
	for (int n = 0; n < N_FRAME; n++)
	{
		MbufAlloc2d(MilSystem[0], (MIL_INT)(MdigInquire(MilDigitizer[0], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[0], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[0][n]);
		MbufAlloc2d(MilSystem[1], (MIL_INT)(MdigInquire(MilDigitizer[1], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[1], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[1][n]);
		MbufAlloc2d(MilSystem[2], (MIL_INT)(MdigInquire(MilDigitizer[2], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[2], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[2][n]);
		MbufAlloc2d(MilSystem[3], (MIL_INT)(MdigInquire(MilDigitizer[3], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[3], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[3][n]);

		if (!MilImageBuf[0][n] | !MilImageBuf[1][n] | !MilImageBuf[2][n] | !MilImageBuf[3][n]) {
			cout << "Allocation Failed\n";
			exit(-1);
		}

		MbufClear(MilImageBuf[0][n], 0L);
		MbufClear(MilImageBuf[1][n], 0L);
		MbufClear(MilImageBuf[2][n], 0L);
		MbufClear(MilImageBuf[3][n], 0L);
	}



	for (int n = 0; n < 4; n++) {
		UserHookData[n].ProcessedImageCount = 0;
		UserHookData[n].remote_flag = MsysInquire(MilSystem[n], M_DISTRIBUTED_MIL_TYPE, M_NULL);
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


	MosPrintf(MIL_TEXT("Press Any Key to Start Process...\n"));
	MosGetch();
	thread t1(load2hostQueue, MilApplication, ref(stop));

	MdigProcess(MilDigitizer[0], MilImageBuf[0], N_FRAME, M_START, M_TRIGGER_FOR_FIRST_GRAB, ProcessingFunction, &UserHookData[0]);
	MdigProcess(MilDigitizer[1], MilImageBuf[1], N_FRAME, M_START, M_TRIGGER_FOR_FIRST_GRAB, ProcessingFunction, &UserHookData[1]);
	MdigProcess(MilDigitizer[2], MilImageBuf[2], N_FRAME, M_START, M_TRIGGER_FOR_FIRST_GRAB, ProcessingFunction, &UserHookData[2]);
	MdigProcess(MilDigitizer[3], MilImageBuf[3], N_FRAME, M_START, M_TRIGGER_FOR_FIRST_GRAB, ProcessingFunction, &UserHookData[3]);


	MosPrintf(MIL_TEXT("Press Any Key to Stop Process...\n"));
	MosGetch();

	stop = true;
	t1.join();

	MdigProcess(MilDigitizer[0], MilImageBuf[0], N_FRAME, M_STOP, M_DEFAULT, ProcessingFunction, &UserHookData[0]);
	MdigProcess(MilDigitizer[1], MilImageBuf[1], N_FRAME, M_STOP, M_DEFAULT, ProcessingFunction, &UserHookData[1]);
	MdigProcess(MilDigitizer[2], MilImageBuf[2], N_FRAME, M_STOP, M_DEFAULT, ProcessingFunction, &UserHookData[2]);
	MdigProcess(MilDigitizer[3], MilImageBuf[3], N_FRAME, M_STOP, M_DEFAULT, ProcessingFunction, &UserHookData[3]);

	MdigInquire(MilDigitizer[0], M_PROCESS_FRAME_COUNT, &ProcessFrameCount[0]);
	MdigInquire(MilDigitizer[1], M_PROCESS_FRAME_COUNT, &ProcessFrameCount[1]);
	MdigInquire(MilDigitizer[2], M_PROCESS_FRAME_COUNT, &ProcessFrameCount[2]);
	MdigInquire(MilDigitizer[3], M_PROCESS_FRAME_COUNT, &ProcessFrameCount[3]);

	MdigInquire(MilDigitizer[0], M_PROCESS_FRAME_MISSED, &ProcessFrameMissedCount[0]);
	MdigInquire(MilDigitizer[1], M_PROCESS_FRAME_MISSED, &ProcessFrameMissedCount[1]);
	MdigInquire(MilDigitizer[2], M_PROCESS_FRAME_MISSED, &ProcessFrameMissedCount[2]);
	MdigInquire(MilDigitizer[3], M_PROCESS_FRAME_MISSED, &ProcessFrameMissedCount[3]);

	MdigInquire(MilDigitizer[0], M_PROCESS_FRAME_RATE, &ProcessFrameRate[0]);
	MdigInquire(MilDigitizer[1], M_PROCESS_FRAME_RATE, &ProcessFrameRate[1]);
	MdigInquire(MilDigitizer[2], M_PROCESS_FRAME_RATE, &ProcessFrameRate[2]);
	MdigInquire(MilDigitizer[3], M_PROCESS_FRAME_RATE, &ProcessFrameRate[3]);

	MosPrintf(MIL_TEXT("First Digitizer: %d frames grabbed at %.1f frames/sec, %d frames missed\n"), (int)ProcessFrameCount[0], ProcessFrameRate[0], ProcessFrameMissedCount[0]);
	MosPrintf(MIL_TEXT("Second Digitizer: %d frames grabbed at %.1f frames/sec, %d frames missed\n"), (int)ProcessFrameCount[1], ProcessFrameRate[1], ProcessFrameMissedCount[1]);
	MosPrintf(MIL_TEXT("Third Digitizer: %d frames grabbed at %.1f frames/sec, %d frames missed\n"), (int)ProcessFrameCount[2], ProcessFrameRate[2], ProcessFrameMissedCount[2]);
	MosPrintf(MIL_TEXT("Fourth Digitizer: %d frames grabbed at %.1f frames/sec, %d frames missed\n"), (int)ProcessFrameCount[3], ProcessFrameRate[3], ProcessFrameMissedCount[3]);


	MosPrintf(MIL_TEXT("Process Done!\n"));
	MosGetch();


	for (int n = 0; n < N_FRAME; n++)
	{
		MbufFree(MilImageBuf[0][n]);
		MbufFree(MilImageBuf[1][n]);
		MbufFree(MilImageBuf[2][n]);
		MbufFree(MilImageBuf[3][n]);
	}


	MbufFree(MilImage[0]); MbufFree(MilImage[1]); MbufFree(MilImage[2]); MbufFree(MilImage[3]);
	MdispFree(MilDisplay[0]); MdispFree(MilDisplay[1]); MdispFree(MilDisplay[2]); MdispFree(MilDisplay[3]);
	MdigFree(MilDigitizer[0]); MdigFree(MilDigitizer[1]); MdigFree(MilDigitizer[2]); MdigFree(MilDigitizer[3]);
	MsysFree(MilSystem[0]); MsysFree(MilSystem[1]); MsysFree(MilSystem[2]); MsysFree(MilSystem[3]);
	MappFree(MilApplication);

	return 0;
}

MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr)
{

	HookDataStruct* UserHookDataPtr = (HookDataStruct*)HookDataPtr;
	MIL_ID SaveBufferId;
	UserHookDataPtr->ProcessedImageCount++;

	MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &SaveBufferId);

	stringstream s_idx;
	s_idx << setw(2) << setfill('0') << UserHookDataPtr->save_idx + (int)UserHookDataPtr->ProcessedImageCount;
	string save_idx = s_idx.str();

	MIL_STRING_STREAM stream;

	if (UserHookDataPtr->remote_flag == M_DMIL_REMOTE)
	{
		stream << MIL_TEXT("remote:///") << MIL_TEXT("C:/output_image/") << stoi(save_idx) << MIL_TEXT("_") << UserHookDataPtr->digit_idx << MIL_TEXT(".tiff");
		MIL_STRING file_name = stream.str();
		MbufSave(file_name, SaveBufferId);

		MIL_STRING_STREAM QueueStream;
		QueueStream << MIL_TEXT("C:/output_image/") << stoi(save_idx) << MIL_TEXT("_") << UserHookDataPtr->digit_idx << MIL_TEXT(".tiff");
		pushQueue(UserHookDataPtr->system_id, QueueStream.str());
	}
	else {
		stream << MIL_TEXT("C:/output_image/") << stoi(save_idx) << MIL_TEXT("_") << UserHookDataPtr->digit_idx << MIL_TEXT(".tiff");
		MIL_STRING file_name = stream.str();
		MbufSave(file_name, SaveBufferId);
	}

	return 0;
}