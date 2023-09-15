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

#define N_CAM 2
#define N_FRAME 45
#define N_THREAD 4
#define MOVE_THREAD 4

using namespace std;
using std::thread;

MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr);
void saveQueue(atomic <bool>& stop);
void pushSaveQueue(MIL_ID GrabImage, MIL_ID system_id, MIL_INT system_location, int save_idx, int digit_idx, double timestamp);
void moveQueue(MIL_ID host_app);
void pushMoveQueue(MIL_ID system_id, MIL_STRING file_name);

#define CHK(FUN) if ((FUN) == M_NULL) {MosPrintf("Failed at %d\n", __LINE__ ); exit(-1);}


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
	double timestamp;
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
	MIL_DOUBLE TimeStamp;
} HookDataStruct;

queue<MoveQueueStruct> move_queue;
queue<SaveQueueStruct> save_queue;
shared_mutex SaveMutex;
shared_mutex MoveMutex;

int MosMain() {
	MIL_ID MilApplication;
	MIL_ID MilSystem[N_CAM];
	MIL_ID MilDisplay[N_CAM];
	MIL_ID MilImage[N_CAM];
	MIL_ID MilDigitizer[N_CAM];
	HookDataStruct UserHookData[N_CAM];
	atomic<bool> stop = false;

	CHK(MappAlloc(M_NULL, M_DEFAULT, &MilApplication));
	MappControl(MilApplication, M_ERROR, M_PRINT_DISABLE);
	MappControlMp(M_DEFAULT, M_MP_USE, M_DEFAULT, M_ENABLE, M_NULL);

	MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV0, M_DEFAULT, &MilSystem[0]);
	MsysControl(MilSystem[0], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[0], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[0]);
	MdigAlloc(MilSystem[0], M_DEV0, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[0]);

	MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV1, M_DEFAULT, &MilSystem[1]);
	MsysControl(MilSystem[1], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
	MdispAlloc(MilSystem[1], M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay[1]);
	MdigAlloc(MilSystem[1], M_DEV0, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer[1]);

	MIL_INT64 Height = 5120;
	MIL_INT64 Width = 5120;
	// MIL_DOUBLE ExposureTime = 400000;
	// MIL_DOUBLE AcquisitionFrameRate = 2;
	// MIL_DOUBLE Gain = 2;
	MIL_DOUBLE ExposureTime = 30000;
	MIL_DOUBLE AcquisitionFrameRate = 30;
	MIL_DOUBLE Gain = 2;

	for (int n = 0; n < N_CAM; n++) {
		MdigControlFeature(MilDigitizer[n], M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
		MdigControl(MilDigitizer[n], M_SOURCE_SIZE_Y, Height);

		MdigControlFeature(MilDigitizer[n], M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
		MdigControl(MilDigitizer[n], M_SOURCE_SIZE_X, Width);

		MdigControlFeature(MilDigitizer[n], M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, MIL_TEXT("Mono8"));

		MdigControlFeature(MilDigitizer[n], M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);

		MdigControlFeature(MilDigitizer[n], M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);

		MdigControlFeature(MilDigitizer[n], M_FEATURE_VALUE, MIL_TEXT("Gain"), M_TYPE_DOUBLE, &Gain);

		CHK(MbufAlloc2d(MilSystem[n], (MIL_INT)(MdigInquire(MilDigitizer[n], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[n], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_DISP + M_GRAB, &MilImage[n]));

		MbufClear(MilImage[n], 0L);

		MdigControlFeature(MilDigitizer[n], M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
		MdigControlFeature(MilDigitizer[n], M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
		MdigControlFeature(MilDigitizer[n], M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
		MdigControlFeature(MilDigitizer[n], M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, MIL_TEXT("RisingEdge"));

		MdigControl(MilDigitizer[n], M_IO_MODE + M_AUX_IO4, M_INPUT);
		MdigControl(MilDigitizer[n], M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
		MdigControl(MilDigitizer[n], M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
	}


	MIL_ID MilImageBuf[N_CAM][N_FRAME];
	for (int n = 0; n < N_FRAME; n++)
	{
		for (int m = 0; m < N_CAM; m++)
		{
			CHK(MbufAlloc2d(MilSystem[m], (MIL_INT)(MdigInquire(MilDigitizer[m], M_SIZE_X, M_NULL)), (MIL_INT)(MdigInquire(MilDigitizer[m], M_SIZE_Y, M_NULL)), 8 + M_UNSIGNED, M_IMAGE + M_GRAB, &MilImageBuf[m][n]));
		}

		for (int m = 0; m < N_CAM; m++)
		{
			MbufClear(MilImageBuf[m][n], 0L);
		}
	}



	for (int n = 0; n < N_CAM; n++) {
		UserHookData[n].ProcessedImageCount = 0;
		UserHookData[n].system_location = MsysInquire(MilSystem[n], M_DISTRIBUTED_MIL_TYPE, M_NULL);
		UserHookData[n].system_id = MsysInquire(MilSystem[n], M_OWNER_APPLICATION, M_NULL);
		UserHookData[n].save_idx = 0;
		UserHookData[n].digit_idx = n + 1;
		UserHookData[n].Height = Height;
		UserHookData[n].Width = Width;
		UserHookData[n].TimeStamp = 0.0;

		MdigControl(MilDigitizer[n], M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
	}


	for (int n = 0; n < N_CAM; n++) {
		MdigProcess(MilDigitizer[n], MilImageBuf[n], N_FRAME, M_START, M_DEFAULT, ProcessingFunction, &UserHookData[n]);
		//MdigProcess(MilDigitizer[n], MilImageBuf[n], N_FRAME, M_START + M_FRAMES_PER_TRIGGER(1), M_TRIGGER_FOR_FIRST_GRAB, ProcessingFunction, &UserHookData[n]);
	}
	MosPrintf(MIL_TEXT("Camera Ready Done!\n"));


	thread t[N_THREAD];
	for (int n = 0; n < N_THREAD; n++)
	{
		t[n] = thread(saveQueue, ref(stop));
	}

	MosPrintf(MIL_TEXT("Press Any Key to Stop Process...\n"));
	MosGetch();

	for (int n = 0; n < N_CAM; n++) {
		MdigProcess(MilDigitizer[n], MilImageBuf[n], N_FRAME, M_STOP, M_DEFAULT, ProcessingFunction, &UserHookData[n]);
	}

	stop = true;
	for (int n = 0; n < N_THREAD; n++)
	{
		t[n].join();
	}

	MosPrintf(MIL_TEXT("Gathering Images...\n\n"));

	clock_t start = clock();

	thread t_[MOVE_THREAD];
	for (int n = 0; n < MOVE_THREAD; n++)
	{
		t_[n] = thread(moveQueue, MilApplication);
	}

	for (int n = 0; n < MOVE_THREAD; n++)
	{
		t_[n].join();
	}

	clock_t finish = clock();

	double duration = (double)(finish - start) / CLOCKS_PER_SEC;

	MosPrintf("%f seconds\n", duration);


	MIL_INT ProcessFrameCount, ProcessFrameMissedCount;
	MIL_DOUBLE ProcessFrameRate;
	for (int n = 0; n < N_CAM; n++) {
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_COUNT, &ProcessFrameCount);
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_MISSED, &ProcessFrameMissedCount);
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_RATE, &ProcessFrameRate);
		MosPrintf(MIL_TEXT("%02dth Digitizer: %d frames grabbed at %.1f frames/sec, %d frames missed\n"), n + 1, ProcessFrameCount, ProcessFrameRate, ProcessFrameMissedCount);
	}

	MosPrintf(MIL_TEXT("Process Done!\n"));
	MosGetch();


	for (int n = 0; n < N_FRAME; n++)
	{
		for (int m = 0; m < N_CAM; m++)
		{
			MbufFree(MilImageBuf[m][n]);
		}
	}

	for (int n = 0; n < N_CAM; n++) {
		MbufFree(MilImage[n]);
		MdispFree(MilDisplay[n]);
		MdigFree(MilDigitizer[n]);
		MsysFree(MilSystem[n]);
	}
	MappFree(MilApplication);

	return 0;
}

MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr)
{

	HookDataStruct* UserHookDataPtr = (HookDataStruct*)HookDataPtr;
	MIL_DOUBLE EventTimeStamp;
	MIL_ID SaveBufferId, QueueId;
	UserHookDataPtr->ProcessedImageCount++;

	CHK(!MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &SaveBufferId));
	MdigGetHookInfo(HookId, M_TIME_STAMP, &EventTimeStamp);

	CHK(MbufClone(SaveBufferId, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_IMAGE + M_HOST_MEMORY + M_OFF_BOARD, M_COPY_SOURCE_DATA, &QueueId));

	pushSaveQueue(QueueId, UserHookDataPtr->system_id, UserHookDataPtr->system_location, UserHookDataPtr->save_idx + (int)UserHookDataPtr->ProcessedImageCount, UserHookDataPtr->digit_idx, (double)EventTimeStamp);

	return 0;
}

void pushSaveQueue(MIL_ID GrabImage, MIL_ID system_id, MIL_INT system_location, int save_idx, int digit_idx, double timestamp)
{
	lock_guard lk(SaveMutex);

	SaveQueueStruct queue_entry = { GrabImage, system_id, system_location, save_idx, digit_idx, timestamp };
	//queue_entry.GrabImage = GrabImage;
	//queue_entry.system_id = system_id;
	//queue_entry.system_location = system_location;
	//queue_entry.save_idx = save_idx;
	//queue_entry.digit_idx = digit_idx;
	//queue_entry.timestamp = timestamp;

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

			stringstream d_idx;
			d_idx << setw(2) << setfill('0') << front_queue.digit_idx;


			MbufSave("C:/output_image/" + s_idx.str() + "_" + d_idx.str() + "_" + to_string(front_queue.timestamp) + ".tiff", front_queue.GrabImage);

		}
		else {
			if (stop) {
				break;
			}

			//MosPrintf(MIL_TEXT("Wait for grabbing...\n"));
			Sleep(500);
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