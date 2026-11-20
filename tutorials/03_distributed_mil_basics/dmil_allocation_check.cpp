// 03a - Distributed MIL: can this PC allocate systems on another PC?
//
// What this chapter adds over chapter 02:
//   A MIL system can live on another PC. The only change in MsysAlloc is the
//   descriptor: instead of M_SYSTEM_RAPIXOCXP, a "dmiltcp://<ip>:<port>/<system>"
//   string names the remote PC. This program does nothing but allocate two local
//   and two remote systems, report success, and free them. Run it first, before
//   the live view, to find connection problems early.
//
// Key MIL functions:
//   MsysAlloc   - with a dmiltcp:// descriptor for the remote boards
//   MsysControl - M_MODIFIED_BUFFER_HOOK_MODE (see the note at the call)
//   MsysInquire - M_DISTRIBUTED_MIL_TYPE, to tell local from remote systems
//
// At run time:
//   Prints whether each allocation worked. Press any key to free and exit.
//   If an allocation fails, the program says which one and exits.

#include <mil.h>

// ---- Configuration --------------------------------------------------------

// Address of the second PC (runs the Distributed MIL server). Replace with yours.
// 57010 is the port used in the original program.
static const MIL_STRING REMOTE_PC = MIL_TEXT("192.168.0.2");
static const int REMOTE_PORT = 57010;

struct SystemSource
{
	MIL_STRING Descriptor;   // M_SYSTEM_RAPIXOCXP or a dmiltcp:// descriptor
	MIL_INT SystemNum;       // board index on the PC that owns the board
};

// "dmiltcp://<host>:<port>/M_SYSTEM_RAPIXOCXP"
static MIL_STRING RemoteRapixoCxp(const MIL_STRING& HostAddress, int Port)
{
	MIL_STRING_STREAM Stream;
	Stream << MIL_TEXT("dmiltcp://") << HostAddress << MIL_TEXT(":") << Port << MIL_TEXT("/M_SYSTEM_RAPIXOCXP");
	return Stream.str();
}

int MosMain()
{
	const MIL_STRING RemoteDescriptor = RemoteRapixoCxp(REMOTE_PC, REMOTE_PORT);

	const SystemSource SOURCES[] = {
		{ M_SYSTEM_RAPIXOCXP, M_DEV0 },   // this PC, first board
		{ M_SYSTEM_RAPIXOCXP, M_DEV1 },   // this PC, second board
		{ RemoteDescriptor,   M_DEV0 },   // other PC, first board
		{ RemoteDescriptor,   M_DEV1 },   // other PC, second board
	};
	const int SYSTEM_COUNT = (int)(sizeof(SOURCES) / sizeof(SOURCES[0]));

	MIL_ID MilApplication = M_NULL;
	MIL_ID MilSystem[4] = { M_NULL, M_NULL, M_NULL, M_NULL };

	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	bool AllAllocated = true;
	for (int i = 0; i < SYSTEM_COUNT; i++)
	{
		MsysAlloc(SOURCES[i].Descriptor.c_str(), SOURCES[i].SystemNum, M_DEFAULT, &MilSystem[i]);
		if (MilSystem[i] == M_NULL)
		{
			// A failed allocation leaves the id at M_NULL.
			MosPrintf(MIL_TEXT("System %d (%s, board %d): allocation FAILED\n"),
				i, SOURCES[i].Descriptor.c_str(), (int)SOURCES[i].SystemNum);
			AllAllocated = false;
			continue;
		}

		// Kept from the original program, applied to every allocated system. It sets how
		// MIL calls hook functions for modified buffers; later chapters rely on hooks.
		MsysControl(MilSystem[i], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);

		bool IsRemote = (MsysInquire(MilSystem[i], M_DISTRIBUTED_MIL_TYPE, M_NULL) == M_DMIL_REMOTE);
		MosPrintf(MIL_TEXT("System %d (board %d): allocated, %s\n"),
			i, (int)SOURCES[i].SystemNum, IsRemote ? MIL_TEXT("remote") : MIL_TEXT("local"));
	}

	if (AllAllocated)
		MosPrintf(MIL_TEXT("System allocate success!\n"));
	MosPrintf(MIL_TEXT("Press any key to free the systems and exit.\n"));
	MosGetch();

	for (int i = 0; i < SYSTEM_COUNT; i++)
	{
		if (MilSystem[i] != M_NULL)
			MsysFree(MilSystem[i]);
	}
	MappFree(MilApplication);

	return AllAllocated ? 0 : 1;
}
