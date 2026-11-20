// capture_common.h
//
// Parts shared by continuous_capture.cpp and single_shot_capture.cpp:
//   - the camera table (10 cameras on 5 PCs)
//   - the per-camera state (system, digitizer, grab buffers, counters)
//   - the allocation / configuration / release steps, in the original order
//   - the save job and move job, and the two worker functions
//
// Data flow on every PC:
//   grab buffer --MbufClone--> SaveJob --SaveWorker--> TIFF file on that PC
//   Remote PCs only:                      SaveWorker --> MoveJob --MoveWorker--> host
//
// Strings use MIL_STRING / MIL_TEXT so the code builds as Unicode or multi-byte.

#pragma once

#include <mil.h>

#include "mil_common.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace capture {

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// Folder (on every PC) where the frames are written. Must end with '/'.
// Remote files are moved into the same path on the host after the run.
static const MIL_STRING OUTPUT_DIR = MIL_TEXT("C:/output_image/");

// Prefix that makes MbufSave write on the PC that owns the image (Distributed MIL).
static const MIL_STRING REMOTE_PREFIX = MIL_TEXT("remote:///");

// Addresses of the four remote PCs (each runs the Distributed MIL server).
// Placeholders: the originals have these masked. Replace with your own.
static const MIL_STRING REMOTE_PC_A = MIL_TEXT("192.168.0.2");
static const MIL_STRING REMOTE_PC_B = MIL_TEXT("192.168.0.3");
static const MIL_STRING REMOTE_PC_C = MIL_TEXT("192.168.0.4");
static const MIL_STRING REMOTE_PC_D = MIL_TEXT("192.168.0.5");

// Camera table, in the order of the originals. The position in this table
// decides the camera number in the file name: index n -> "_<n+1>" (1-based).
//
// ASSUMPTION: the originals mask all four remote addresses (***.***.***.***), so
// it cannot be told which remote entries belong to the same PC. The table assumes
// consecutive pairs (entries 2+3, 4+5, 6+7, 8+9), which gives each remote PC
// exactly one M_DEV0 board and one M_DEV1 board. Note the original order is not
// always DEV0 then DEV1: entries 4 and 5 are DEV1 then DEV0.
//
// Every board has a single camera on its digitizer M_DEV0.
static const std::vector<milcam::CameraSource> CAMERAS = {
	{ M_SYSTEM_RAPIXOCXP,                   M_DEV0, M_DEV0 },  // camera 1  : host,  board 0
	{ M_SYSTEM_RAPIXOCXP,                   M_DEV1, M_DEV0 },  // camera 2  : host,  board 1
	{ milcam::RemoteRapixoCxp(REMOTE_PC_A), M_DEV0, M_DEV0 },  // camera 3  : PC A,  board 0
	{ milcam::RemoteRapixoCxp(REMOTE_PC_A), M_DEV1, M_DEV0 },  // camera 4  : PC A,  board 1
	{ milcam::RemoteRapixoCxp(REMOTE_PC_B), M_DEV1, M_DEV0 },  // camera 5  : PC B,  board 1
	{ milcam::RemoteRapixoCxp(REMOTE_PC_B), M_DEV0, M_DEV0 },  // camera 6  : PC B,  board 0
	{ milcam::RemoteRapixoCxp(REMOTE_PC_C), M_DEV0, M_DEV0 },  // camera 7  : PC C,  board 0
	{ milcam::RemoteRapixoCxp(REMOTE_PC_C), M_DEV1, M_DEV0 },  // camera 8  : PC C,  board 1
	{ milcam::RemoteRapixoCxp(REMOTE_PC_D), M_DEV0, M_DEV0 },  // camera 9  : PC D,  board 0
	{ milcam::RemoteRapixoCxp(REMOTE_PC_D), M_DEV1, M_DEV0 },  // camera 10 : PC D,  board 1
};

// ---------------------------------------------------------------------------
// Jobs and queues
// ---------------------------------------------------------------------------

// One grabbed frame waiting to be written to disk.
struct SaveJob
{
	MIL_ID Image = M_NULL;     // host-memory copy of the frame (freed by the save worker)
	MIL_ID OwnerApp = M_NULL;  // application that owns the image (the remote PC's, or the host's)
	bool Remote = false;       // true if the image lives on a remote PC
	int FrameIndex = 0;        // 1-based, goes into the file name
	int CameraIndex = 0;       // 1-based, goes into the file name
};

// One file on a remote PC waiting to be moved to the host.
struct MoveJob
{
	MIL_ID OwnerApp = M_NULL;  // application of the PC that holds the file
	MIL_STRING FileName;       // same path on the remote PC and on the host
};

typedef milcam::WorkQueue<SaveJob> SaveQueue;
typedef milcam::WorkQueue<MoveJob> MoveQueue;

// ---------------------------------------------------------------------------
// Per-camera state
// ---------------------------------------------------------------------------

// Handed to the MdigProcess hook (continuous) or used by the camera's capture
// thread (single shot).
struct CameraHookData
{
	std::atomic<MIL_INT> FrameCount{ 0 };  // frames handled so far
	MIL_ID OwnerApp = M_NULL;
	bool Remote = false;
	int CameraIndex = 0;                   // 1-based
	SaveQueue* Saver = nullptr;
};

struct CameraRig
{
	MIL_ID System = M_NULL;
	MIL_ID Digitizer = M_NULL;
	std::vector<MIL_ID> Buffers;           // grab buffers, allocated on System
	CameraHookData Hook;
};

// ---------------------------------------------------------------------------
// Setup / teardown, one phase at a time over all cameras (as in the originals)
// ---------------------------------------------------------------------------

// Systems and digitizers. M_MULTI_THREAD lets the hook run on several threads.
inline void AllocSystemsAndDigitizers(std::vector<CameraRig>& Rigs)
{
	for (size_t n = 0; n < CAMERAS.size(); n++)
	{
		Rigs[n].System = milcam::AllocSystem(CAMERAS[n]);
		MsysControl(Rigs[n].System, M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);
		Rigs[n].Digitizer = milcam::AllocDigitizer(Rigs[n].System, CAMERAS[n]);
	}
}

// Image format and exposure, then hardware trigger routing.
inline void ConfigureCameras(std::vector<CameraRig>& Rigs, const milcam::CameraSettings& Settings)
{
	for (CameraRig& Rig : Rigs)
		milcam::ApplyCameraSettings(Rig.Digitizer, Settings);
	for (CameraRig& Rig : Rigs)
		milcam::EnableExternalTrigger(Rig.Digitizer);
}

inline void AllocGrabBuffers(std::vector<CameraRig>& Rigs, int BufferCount)
{
	for (CameraRig& Rig : Rigs)
		Rig.Buffers.assign(BufferCount, M_NULL);
	for (int b = 0; b < BufferCount; b++)
		for (CameraRig& Rig : Rigs)
			Rig.Buffers[b] = milcam::AllocMono8Buffer(Rig.System, Rig.Digitizer, M_IMAGE + M_GRAB);
}

inline void InitHookData(std::vector<CameraRig>& Rigs, SaveQueue& Saver)
{
	for (size_t n = 0; n < Rigs.size(); n++)
	{
		Rigs[n].Hook.FrameCount = 0;
		Rigs[n].Hook.Remote = milcam::IsRemoteSystem(Rigs[n].System);
		Rigs[n].Hook.OwnerApp = (MIL_ID)MsysInquire(Rigs[n].System, M_OWNER_APPLICATION, M_NULL);
		Rigs[n].Hook.CameraIndex = (int)n + 1;
		Rigs[n].Hook.Saver = &Saver;
	}
}

inline void ResetMissedFrameCounters(std::vector<CameraRig>& Rigs)
{
	for (CameraRig& Rig : Rigs)
		MdigControl(Rig.Digitizer, M_GRAB_FRAME_MISSED_RESET, M_DEFAULT);
}

// Reverse order of allocation: buffers, digitizers, systems.
inline void FreeRigs(std::vector<CameraRig>& Rigs)
{
	for (int b = (int)Rigs[0].Buffers.size() - 1; b >= 0; b--)
		for (size_t n = Rigs.size(); n-- > 0;)
			MbufFree(Rigs[n].Buffers[b]);
	for (size_t n = Rigs.size(); n-- > 0;)
		MdigFree(Rigs[n].Digitizer);
	for (size_t n = Rigs.size(); n-- > 0;)
		MsysFree(Rigs[n].System);
}

// ---------------------------------------------------------------------------
// Producing and consuming jobs
// ---------------------------------------------------------------------------

// Copies a grabbed buffer into a host-memory image (so the grab buffer can be
// reused right away) and queues the copy for saving. On a remote PC the copy
// is made on that PC, so no pixel data crosses the network here.
inline void QueueFrameForSave(CameraHookData& Camera, MIL_ID GrabBuffer, int FrameIndex)
{
	MIL_ID CopyId = M_NULL;
	MbufClone(GrabBuffer, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_DEFAULT, M_IMAGE + M_HOST_MEMORY + M_OFF_BOARD, M_COPY_SOURCE_DATA, &CopyId);
	if (CopyId == M_NULL)
		return;   // out of memory: this frame is lost, but capture goes on

	SaveJob Job;
	Job.Image = CopyId;
	Job.OwnerApp = Camera.OwnerApp;
	Job.Remote = Camera.Remote;
	Job.FrameIndex = FrameIndex;
	Job.CameraIndex = Camera.CameraIndex;
	Camera.Saver->Push(Job);
}

// Runs until the save queue is closed and empty. Local frames are written
// directly. Remote frames are written on their own PC ("remote:///") and a
// MoveJob is queued so the file can be gathered on the host afterwards.
inline void SaveWorker(SaveQueue& Saver, MoveQueue& Mover)
{
	SaveJob Job;
	while (Saver.Pop(Job))
	{
		MIL_STRING Path = milcam::FrameFileName(OUTPUT_DIR, Job.FrameIndex, Job.CameraIndex);

		if (Job.Remote)
		{
			MIL_STRING RemotePath = REMOTE_PREFIX + Path;
			MbufSave(RemotePath, Job.Image);
			MbufFree(Job.Image);

			MoveJob Move;
			Move.OwnerApp = Job.OwnerApp;
			Move.FileName = Path;
			Mover.Push(Move);
		}
		else
		{
			MbufSave(Path, Job.Image);
			MbufFree(Job.Image);
		}
	}
}

// Moves each queued file from its remote PC to the same path on the host.
inline void MoveWorker(MoveQueue& Mover, MIL_ID HostApp)
{
	MoveJob Job;
	while (Mover.Pop(Job))
	{
		MappFileOperation(Job.OwnerApp, Job.FileName, HostApp, Job.FileName, M_FILE_MOVE, M_DEFAULT, M_NULL);
	}
}

// Closes the move queue, runs MoveThreads workers until it is drained, and
// prints how long that took. Call after the save workers have been joined.
inline void GatherImages(MoveQueue& Mover, MIL_ID HostApp, int MoveThreads)
{
	MosPrintf(MIL_TEXT("Gathering Images...\n"));
	std::chrono::steady_clock::time_point Start = std::chrono::steady_clock::now();

	Mover.Close();
	std::vector<std::thread> Workers;
	for (int n = 0; n < MoveThreads; n++)
		Workers.emplace_back(MoveWorker, std::ref(Mover), HostApp);
	for (std::thread& Worker : Workers)
		Worker.join();

	double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	MosPrintf(MIL_TEXT("Moving images took %.2f seconds\n"), Seconds);
}

}  // namespace capture
