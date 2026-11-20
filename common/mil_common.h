// mil_common.h
//
// Small header-only helpers shared by the tutorials (chapter 04 onwards) and
// by apps/. Chapters 01-03 deliberately do NOT use this header so that the raw
// MIL calls stay visible; everything wrapped here is first shown there.
//
// Hardware this was written for:
//   - Matrox Rapixo CXP frame grabbers (CoaXPress 2.0, CXP-12)
//   - CoaXPress area-scan cameras (5120 x 5120, Mono8)
//   - Several PCs linked with Distributed MIL (DMIL)
//
// All strings use MIL_STRING / MIL_TEXT so the code builds with either the
// Unicode or the multi-byte character set.

#pragma once

#include <mil.h>

#include <condition_variable>
#include <cstdlib>
#include <iomanip>
#include <mutex>
#include <queue>
#include <vector>

namespace milcam {

// ---------------------------------------------------------------------------
// Configuration types
// ---------------------------------------------------------------------------

// Where one camera lives: which MIL system (frame grabber) and which
// digitizer (connection) on that system.
struct CameraSource
{
	MIL_STRING SystemDescriptor;  // M_SYSTEM_RAPIXOCXP or a dmiltcp:// descriptor
	MIL_INT SystemNum;            // board index on that PC: M_DEV0, M_DEV1, ...
	MIL_INT DigitizerNum;         // digitizer index on that board: M_DEV0, ...
};

// GenICam features written to every camera.
struct CameraSettings
{
	MIL_INT64 Width = 5120;
	MIL_INT64 Height = 5120;
	MIL_STRING PixelFormat = MIL_TEXT("Mono8");
	MIL_DOUBLE ExposureTime = 60000;        // microseconds
	MIL_DOUBLE AcquisitionFrameRate = 15;   // frames per second
	MIL_DOUBLE Gain = 1;
};

// Builds the system descriptor of a Rapixo CXP board on another PC.
// The remote PC must be running the Distributed MIL server (default port 57010).
inline MIL_STRING RemoteRapixoCxp(const MIL_STRING& HostAddress, int Port = 57010)
{
	MIL_STRING_STREAM Stream;
	Stream << MIL_TEXT("dmiltcp://") << HostAddress << MIL_TEXT(":") << Port << MIL_TEXT("/M_SYSTEM_RAPIXOCXP");
	return Stream.str();
}

// ---------------------------------------------------------------------------
// Allocation helpers
// ---------------------------------------------------------------------------

// Stops the program with a message if a MIL allocation returned M_NULL.
inline void RequireAllocated(MIL_ID Id, MIL_CONST_TEXT_PTR What)
{
	if (Id == M_NULL)
	{
		MosPrintf(MIL_TEXT("Allocation failed: %s\n"), What);
		MosPrintf(MIL_TEXT("Press any key to exit.\n"));
		MosGetch();
		std::exit(EXIT_FAILURE);
	}
}

inline MIL_ID AllocSystem(const CameraSource& Source)
{
	MIL_ID MilSystem = M_NULL;
	MsysAlloc(Source.SystemDescriptor.c_str(), Source.SystemNum, M_DEFAULT, &MilSystem);
	RequireAllocated(MilSystem, Source.SystemDescriptor.c_str());
	return MilSystem;
}

inline MIL_ID AllocDigitizer(MIL_ID MilSystem, const CameraSource& Source)
{
	MIL_ID MilDigitizer = M_NULL;
	MdigAlloc(MilSystem, Source.DigitizerNum, MIL_TEXT("M_DEFAULT"), M_DEFAULT, &MilDigitizer);
	RequireAllocated(MilDigitizer, MIL_TEXT("digitizer"));
	return MilDigitizer;
}

inline MIL_ID AllocDisplay(MIL_ID MilSystem)
{
	MIL_ID MilDisplay = M_NULL;
	MdispAlloc(MilSystem, M_DEFAULT, MIL_TEXT("M_DEFAULT"), M_WINDOWED, &MilDisplay);
	RequireAllocated(MilDisplay, MIL_TEXT("display"));
	return MilDisplay;
}

// Allocates an 8-bit mono buffer the size of the digitizer's image and clears it.
// Attribute is e.g. M_IMAGE + M_GRAB, or M_IMAGE + M_GRAB + M_DISP.
inline MIL_ID AllocMono8Buffer(MIL_ID MilSystem, MIL_ID MilDigitizer, MIL_INT64 Attribute)
{
	MIL_ID MilImage = M_NULL;
	MbufAlloc2d(MilSystem,
		(MIL_INT)(MdigInquire(MilDigitizer, M_SIZE_X, M_NULL)),
		(MIL_INT)(MdigInquire(MilDigitizer, M_SIZE_Y, M_NULL)),
		8 + M_UNSIGNED, Attribute, &MilImage);
	RequireAllocated(MilImage, MIL_TEXT("image buffer"));
	MbufClear(MilImage, 0L);
	return MilImage;
}

// ---------------------------------------------------------------------------
// Camera configuration helpers
// ---------------------------------------------------------------------------

// Writes the image size, pixel format, exposure, frame rate and gain to the
// camera, and tells the frame grabber the matching source size.
inline void ApplyCameraSettings(MIL_ID MilDigitizer, const CameraSettings& Settings)
{
	MIL_INT64 Height = Settings.Height;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("Height"), M_TYPE_INT64, &Height);
	MdigControl(MilDigitizer, M_SOURCE_SIZE_Y, Height);

	MIL_INT64 Width = Settings.Width;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("Width"), M_TYPE_INT64, &Width);
	MdigControl(MilDigitizer, M_SOURCE_SIZE_X, Width);

	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("PixelFormat"), M_TYPE_STRING, Settings.PixelFormat.c_str());

	MIL_DOUBLE ExposureTime = Settings.ExposureTime;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);

	MIL_DOUBLE AcquisitionFrameRate = Settings.AcquisitionFrameRate;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("AcquisitionFrameRate"), M_TYPE_DOUBLE, &AcquisitionFrameRate);

	MIL_DOUBLE Gain = Settings.Gain;
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("Gain"), M_TYPE_DOUBLE, &Gain);
}

// Makes the camera expose on an external pulse instead of free-running.
//
// The pulse arrives on an auxiliary input of the frame grabber (AuxInput,
// e.g. M_AUX_IO4). The grabber forwards it to the camera over the CoaXPress
// link ("LinkTrigger0"), so no separate trigger cable to the camera is needed.
// Feeding the same pulse to every grabber is what synchronizes the cameras.
inline void EnableExternalTrigger(MIL_ID MilDigitizer, MIL_INT64 AuxInput = M_AUX_IO4, MIL_CONST_TEXT_PTR TriggerActivation = MIL_TEXT("RisingEdge"))
{
	// Camera side: start exposure on the CoaXPress link trigger.
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
	MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, TriggerActivation);

	// Frame grabber side: route the auxiliary input to the link trigger.
	MdigControl(MilDigitizer, M_IO_MODE + AuxInput, M_INPUT);
	MdigControl(MilDigitizer, M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
	MdigControl(MilDigitizer, M_IO_SOURCE + M_TL_TRIGGER, AuxInput);
}

// True if the system was allocated on another PC through Distributed MIL.
inline bool IsRemoteSystem(MIL_ID MilSystem)
{
	return MsysInquire(MilSystem, M_DISTRIBUTED_MIL_TYPE, M_NULL) == M_DMIL_REMOTE;
}

// ---------------------------------------------------------------------------
// File naming
// ---------------------------------------------------------------------------

// "<Directory><FFFF>_<CC><Extension>", e.g. "C:/output_image/0012_03.tiff"
// for frame 12 of camera 3. Directory must end with '/'.
inline MIL_STRING FrameFileName(const MIL_STRING& Directory, int FrameIndex, int CameraIndex, const MIL_STRING& Extension = MIL_TEXT(".tiff"))
{
	MIL_STRING_STREAM Stream;
	Stream << Directory
		<< std::setw(4) << std::setfill(MIL_TEXT('0')) << FrameIndex
		<< MIL_TEXT("_")
		<< std::setw(2) << std::setfill(MIL_TEXT('0')) << CameraIndex
		<< Extension;
	return Stream.str();
}

// ---------------------------------------------------------------------------
// Thread-safe work queue
// ---------------------------------------------------------------------------

// Producers call Push(); worker threads loop on Pop() until it returns false.
// Pop() blocks while the queue is empty, and returns false only after Close()
// has been called and every remaining item has been handed out.
template <typename T>
class WorkQueue
{
public:
	void Push(T Item)
	{
		{
			std::lock_guard<std::mutex> Lock(Mutex_);
			Items_.push(std::move(Item));
		}
		NotEmpty_.notify_one();
	}

	bool Pop(T& Item)
	{
		std::unique_lock<std::mutex> Lock(Mutex_);
		NotEmpty_.wait(Lock, [this] { return !Items_.empty() || Closed_; });
		if (Items_.empty())
			return false;
		Item = std::move(Items_.front());
		Items_.pop();
		return true;
	}

	// Non-blocking: returns false immediately if the queue is empty.
	bool TryPop(T& Item)
	{
		std::lock_guard<std::mutex> Lock(Mutex_);
		if (Items_.empty())
			return false;
		Item = std::move(Items_.front());
		Items_.pop();
		return true;
	}

	// No more items will be pushed; lets blocked workers drain and exit.
	void Close()
	{
		{
			std::lock_guard<std::mutex> Lock(Mutex_);
			Closed_ = true;
		}
		NotEmpty_.notify_all();
	}

private:
	std::mutex Mutex_;
	std::condition_variable NotEmpty_;
	std::queue<T> Items_;
	bool Closed_ = false;
};

}  // namespace milcam
