// serial_trigger_capture.cpp
//
// What this step adds over chapter 07 (threaded save queue):
//   Until now somebody had to switch the trigger pulses on and off by hand.
//   Here the program does it: the external trigger generator is driven over a
//   serial (COM) port, so "start grabbing, fire pulses for a fixed time, stop"
//   becomes one key press and the number of frames is repeatable.
//
// Nothing new on the MIL side. The new part is plain Win32 serial I/O:
//   CreateFile on "COMx", SetCommTimeouts, GetCommState / SetCommState,
//   SetupComm, PurgeComm, WriteFile, CloseHandle.
//
// The commands are those of the trigger generator used in this project. They
// are ASCII, terminated by a carriage return. Their meaning below is inferred
// from how the original program used them, so check the manual of your own
// generator:
//   "chmd 0"   select the channel/operating mode
//   "rfq 30"   pulse frequency, 30 Hz (keep it equal to FRAME_RATE)
//   "oen"      output enable:  pulses start
//   "ode"      output disable: pulses stop
//
// At run time:
//   - The program opens the COM port, configures the generator with its output
//     disabled, and starts MdigProcess on every camera (which now waits).
//   - Press a key: pulses are enabled for PULSE_TIME_MS, then disabled.
//   - The program stops MdigProcess, waits for the save workers, and prints the
//     time that took and the per-camera statistics.
//   - Files are saved on the PC that owns each camera, as in chapter 07:
//     C:/output_image/FFFF_CC.tiff

#include <mil.h>

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#include "mil_common.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// Address of the PC that holds the cameras (runs the Distributed MIL server).
// Replace with yours. The original used two boards of one remote PC.
static const MIL_STRING REMOTE_PC = MIL_TEXT("192.168.0.2");

static const std::vector<milcam::CameraSource> CAMERAS = {
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV0, M_DEV0 },
	{ milcam::RemoteRapixoCxp(REMOTE_PC), M_DEV1, M_DEV0 },
};

static const int N_FRAME = 50;     // grab buffers per camera
static const int N_THREAD = 10;    // save workers
static const MIL_STRING OUTPUT_DIR = MIL_TEXT("C:/output_image/");
static const MIL_STRING REMOTE_PREFIX = MIL_TEXT("remote:///");

static const MIL_DOUBLE EXPOSURE_TIME = 30000;   // microseconds
static const MIL_DOUBLE FRAME_RATE = 30;         // frames per second

// Serial port of the trigger generator.
static const char* const TRIGGER_PORT = "COM4";
static const DWORD TRIGGER_BAUD_RATE = CBR_115200;
static const char* const CMD_MODE = "chmd 0\r";
static const char* const CMD_FREQUENCY = "rfq 30\r";
static const char* const CMD_OUTPUT_ENABLE = "oen\r";
static const char* const CMD_OUTPUT_DISABLE = "ode\r";

static const MIL_INT PULSE_TIME_MS = 1000;       // how long the pulses stay on

// ---------------------------------------------------------------------------
// Trigger generator (serial port)
// ---------------------------------------------------------------------------

// Opens the COM port: 8 data bits, one stop bit, blocking writes.
// Exits the program on failure.
static HANDLE OpenTriggerPort()
{
	// The A (narrow) version is called explicitly so that this builds under
	// both the Unicode and the multi-byte character set.
	HANDLE Port = CreateFileA(TRIGGER_PORT, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (Port == INVALID_HANDLE_VALUE)
	{
		MosPrintf(MIL_TEXT("CreateFile (Connect to Trigger) Fail...\n"));
		std::exit(EXIT_FAILURE);
	}
	MosPrintf(MIL_TEXT("CreateFile (Connect to Trigger) Done!\n"));

	COMMTIMEOUTS CommTimeOuts;
	CommTimeOuts.ReadIntervalTimeout = INFINITE;
	CommTimeOuts.ReadTotalTimeoutMultiplier = 0;
	CommTimeOuts.ReadTotalTimeoutConstant = IGNORE;
	CommTimeOuts.WriteTotalTimeoutMultiplier = 0;
	CommTimeOuts.WriteTotalTimeoutConstant = INFINITE;
	SetCommTimeouts(Port, &CommTimeOuts);

	DCB Dcb;
	Dcb.DCBlength = sizeof(DCB);
	GetCommState(Port, &Dcb);
	Dcb.BaudRate = TRIGGER_BAUD_RATE;
	Dcb.ByteSize = 8;
	Dcb.StopBits = ONESTOPBIT;

	if (!SetCommState(Port, &Dcb) || !SetupComm(Port, 4096, 4096) || !PurgeComm(Port, PURGE_TXCLEAR | PURGE_RXCLEAR))
	{
		CloseHandle(Port);
		MosPrintf(MIL_TEXT("SetupComm (Setup Communication Trigger) Fail...\n"));
		std::exit(EXIT_FAILURE);
	}
	return Port;
}

// Sends one command. Exits the program on failure.
static void SendTriggerCommand(HANDLE Port, const char* Command)
{
	DWORD BytesWritten = 0;
	if (!WriteFile(Port, (LPCVOID)Command, (DWORD)std::strlen(Command), &BytesWritten, NULL))
	{
		CloseHandle(Port);
		MosPrintf(MIL_TEXT("WriteFile (Send To Trigger) Fail...\n"));
		std::exit(EXIT_FAILURE);
	}
}

// ---------------------------------------------------------------------------
// Capture pipeline (same as chapter 07)
// ---------------------------------------------------------------------------

struct SaveJob
{
	MIL_ID Image;       // the clone; the worker frees it
	bool Remote;        // buffer lives on a remote PC
	int FrameIndex;
	int CameraIndex;
};

struct HookData
{
	std::atomic<int> ProcessedImageCount{ 0 };
	bool Remote = false;
	int CameraIndex = 0;
	milcam::WorkQueue<SaveJob>* Queue = nullptr;
};

static MIL_INT MFTYPE ProcessingFunction(MIL_INT HookType, MIL_ID HookId, void* HookDataPtr)
{
	HookData* Data = (HookData*)HookDataPtr;
	int FrameIndex = ++Data->ProcessedImageCount;

	MIL_ID GrabbedBuffer = M_NULL;
	MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &GrabbedBuffer);

	MIL_ID Clone = M_NULL;
	MbufClone(GrabbedBuffer, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_IMAGE + M_HOST_MEMORY + M_OFF_BOARD, M_COPY_SOURCE_DATA, &Clone);
	if (Clone == M_NULL)
		return 0;   // out of memory: this frame is lost, but the hook stays alive

	Data->Queue->Push(SaveJob{ Clone, Data->Remote, FrameIndex, Data->CameraIndex });
	return 0;
}

static void SaveWorker(milcam::WorkQueue<SaveJob>& Queue)
{
	SaveJob Job;
	while (Queue.Pop(Job))
	{
		MIL_STRING FileName = milcam::FrameFileName(OUTPUT_DIR, Job.FrameIndex, Job.CameraIndex);
		if (Job.Remote)
			FileName = REMOTE_PREFIX + FileName;   // saved on the remote PC's disk

		MbufSave(FileName, Job.Image);
		MbufFree(Job.Image);
	}
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int MosMain(void)
{
	const int CameraCount = (int)CAMERAS.size();

	milcam::CameraSettings Settings;
	Settings.ExposureTime = EXPOSURE_TIME;
	Settings.AcquisitionFrameRate = FRAME_RATE;

	MIL_ID MilApplication = M_NULL;
	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);
	MappControlMp(M_DEFAULT, M_MP_USE, M_DEFAULT, M_ENABLE, M_NULL);

	std::vector<MIL_ID> MilSystem(CameraCount);
	std::vector<MIL_ID> MilDigitizer(CameraCount);
	std::vector<std::vector<MIL_ID>> GrabBuffers(CameraCount, std::vector<MIL_ID>(N_FRAME));
	std::vector<HookData> Hooks(CameraCount);
	milcam::WorkQueue<SaveJob> SaveQueue;

	// Unlike chapters 06-10, the original of this program left the hook mode of
	// the systems at its default (the M_MULTI_THREAD line was commented out).
	for (int n = 0; n < CameraCount; n++)
	{
		MilSystem[n] = milcam::AllocSystem(CAMERAS[n]);
		MilDigitizer[n] = milcam::AllocDigitizer(MilSystem[n], CAMERAS[n]);
	}

	for (int n = 0; n < CameraCount; n++)
	{
		milcam::ApplyCameraSettings(MilDigitizer[n], Settings);
		milcam::EnableExternalTrigger(MilDigitizer[n]);
	}

	for (int f = 0; f < N_FRAME; f++)
		for (int n = 0; n < CameraCount; n++)
			GrabBuffers[n][f] = milcam::AllocMono8Buffer(MilSystem[n], MilDigitizer[n], M_IMAGE + M_GRAB);

	for (int n = 0; n < CameraCount; n++)
	{
		Hooks[n].Remote = milcam::IsRemoteSystem(MilSystem[n]);
		Hooks[n].CameraIndex = n + 1;
		Hooks[n].Queue = &SaveQueue;
	}

	// Configure the generator and make sure it is silent before the cameras
	// start listening, so the first pulse every camera sees is the same one.
	HANDLE TriggerPort = OpenTriggerPort();
	SendTriggerCommand(TriggerPort, CMD_MODE);
	SendTriggerCommand(TriggerPort, CMD_FREQUENCY);
	SendTriggerCommand(TriggerPort, CMD_OUTPUT_DISABLE);

	std::vector<std::thread> Workers;
	for (int n = 0; n < N_THREAD; n++)
		Workers.emplace_back(SaveWorker, std::ref(SaveQueue));

	// The cameras only expose on a trigger pulse and the generator is silent,
	// so nothing is grabbed until the output is enabled below.
	for (int n = 0; n < CameraCount; n++)
		MdigProcess(MilDigitizer[n], GrabBuffers[n].data(), N_FRAME, M_START, M_DEFAULT, ProcessingFunction, &Hooks[n]);

	MosPrintf(MIL_TEXT("Press Any KEY to start capturing\n"));
	MosGetch();

	SendTriggerCommand(TriggerPort, CMD_OUTPUT_ENABLE);
	MosSleep(PULSE_TIME_MS);
	SendTriggerCommand(TriggerPort, CMD_OUTPUT_DISABLE);

	const std::chrono::steady_clock::time_point Start = std::chrono::steady_clock::now();

	CloseHandle(TriggerPort);

	// Stop every camera, then close the queue, then join the workers.
	for (int n = 0; n < CameraCount; n++)
		MdigProcess(MilDigitizer[n], GrabBuffers[n].data(), N_FRAME, M_STOP, M_DEFAULT, ProcessingFunction, &Hooks[n]);

	SaveQueue.Close();
	for (std::thread& Worker : Workers)
		Worker.join();

	// Time from the last pulse until every frame is on disk.
	const std::chrono::duration<double> Elapsed = std::chrono::steady_clock::now() - Start;
	MosPrintf(MIL_TEXT("Saving finished %.2f seconds after the pulses stopped\n"), Elapsed.count());

	for (int n = 0; n < CameraCount; n++)
	{
		MIL_INT FrameCount = 0;
		MIL_INT FrameMissed = 0;
		MIL_DOUBLE FrameRate = 0;
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_COUNT, &FrameCount);
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_MISSED, &FrameMissed);
		MdigInquire(MilDigitizer[n], M_PROCESS_FRAME_RATE, &FrameRate);
		MosPrintf(MIL_TEXT("Camera %d: %d frames grabbed at %.1f frames/sec, %d frames missed\n"),
			n + 1, (int)FrameCount, FrameRate, (int)FrameMissed);
	}

	MosPrintf(MIL_TEXT("Process Done!\n"));
	MosGetch();

	for (int n = 0; n < CameraCount; n++)
		for (int f = 0; f < N_FRAME; f++)
			MbufFree(GrabBuffers[n][f]);

	for (int n = 0; n < CameraCount; n++)
	{
		MdigFree(MilDigitizer[n]);
		MsysFree(MilSystem[n]);
	}
	MappFree(MilApplication);

	return 0;
}
