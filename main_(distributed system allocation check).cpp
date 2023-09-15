#include <mil.h>
#include <iostream>

using namespace std;

int MosMain() {
	MIL_ID MilApplication;
	MIL_ID MilSystem[4];

	MappAlloc(M_NULL, M_DEFAULT, &MilApplication);

	MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV0, M_DEFAULT, &MilSystem[0]);
	MsysControl(MilSystem[0], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);

	MsysAlloc(M_SYSTEM_RAPIXOCXP, M_DEV1, M_DEFAULT, &MilSystem[1]);
	MsysControl(MilSystem[1], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV0, M_DEFAULT, &MilSystem[2]);
	MsysControl(MilSystem[2], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);

	MsysAlloc(MIL_TEXT("dmiltcp://***.***.***.***:57010/M_SYSTEM_RAPIXOCXP"), M_DEV1, M_DEFAULT, &MilSystem[3]);
	MsysControl(MilSystem[3], M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD);




	MosPrintf(MIL_TEXT("System allocate success!\n"));
	MosGetch();


	MsysFree(MilSystem[0]); MsysFree(MilSystem[1]); MsysFree(MilSystem[2]); MsysFree(MilSystem[3]);
	MappFree(MilApplication);

	return 0;
}