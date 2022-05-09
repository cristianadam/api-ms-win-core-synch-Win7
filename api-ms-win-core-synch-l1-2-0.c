#include <Windows.h>
#include <string.h>

typedef struct _ADDR_CVAR_ASSOC {
	volatile VOID *Address;
	CONDITION_VARIABLE CVar;
	UINT NumberOfWaiters; // number of waiters on the address Address
} ADDR_CVAR_ASSOC, *PADDR_CVAR_ASSOC;

#define AddrCVarAssocTblSize ((SIZE_T)256)
CRITICAL_SECTION AddrCVarAssocTblLock;
PADDR_CVAR_ASSOC AddrCVarAssocTbl[AddrCVarAssocTblSize];

BOOL WINAPI DllMain(
	HINSTANCE hInstDLL,
	DWORD fdwReason,
	LPVOID lpReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		memset(AddrCVarAssocTbl, 0, sizeof(AddrCVarAssocTbl));
		InitializeCriticalSection(&AddrCVarAssocTblLock);
		break;
	case DLL_PROCESS_DETACH:
		DeleteCriticalSection(&AddrCVarAssocTblLock);
		break;
	}

	return TRUE;
}

// Return TRUE if memory is the same. FALSE if memory is different.
static BOOL CompareVolatileMemory(const volatile void *A1, const void *A2, size_t size)
{
	switch (size) {
	case 1:		return (*(const UINT8*)A1 == *(const UINT8*)A2);
	case 2:		return (*(const UINT16*)A1 == *(const UINT16*)A2);
	case 4:		return (*(const UINT32*)A1 == *(const UINT32*)A2);
	case 8:		return (*(const UINT64*)A1 == *(const UINT64*)A2);
	default:	return FALSE;
	}
}

BOOL WINAPI WaitOnAddress(
	_In_ volatile VOID *Address,
	_In_ PVOID CompareAddress,
	_In_ SIZE_T AddressSize,
	_In_ DWORD dwMilliseconds)
{
	SIZE_T idxAddrCVarAssoc;
	SIZE_T availableNullSlot;
	BOOL ReturnValue;
	DWORD SleepConditionVariableCSError;

	if (!Address || !CompareAddress) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	if (!(AddressSize == 1 || AddressSize == 2 || AddressSize == 4 || AddressSize == 8)) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	EnterCriticalSection(&AddrCVarAssocTblLock);
	if (!CompareVolatileMemory(Address, CompareAddress, AddressSize)) {
		LeaveCriticalSection(&AddrCVarAssocTblLock);
		SetLastError(ERROR_SUCCESS);
		return TRUE;
	}

	// look for first slot in AddrCVarAssocTbl which contains either the correct Address
	// or an available NULL slot if we can't find that
	for (idxAddrCVarAssoc = 0; idxAddrCVarAssoc < AddrCVarAssocTblSize; ++idxAddrCVarAssoc) {
		if (AddrCVarAssocTbl[idxAddrCVarAssoc] && AddrCVarAssocTbl[idxAddrCVarAssoc]->Address == Address) {
			break;
		} else if (AddrCVarAssocTbl[idxAddrCVarAssoc] == NULL) {
			availableNullSlot = idxAddrCVarAssoc;
		}
	}

	if (!(AddrCVarAssocTbl[idxAddrCVarAssoc] && AddrCVarAssocTbl[idxAddrCVarAssoc]->Address == Address)) {
		// Need to allocate and populate a new entry in the table
		idxAddrCVarAssoc = availableNullSlot;
		AddrCVarAssocTbl[idxAddrCVarAssoc] = (PADDR_CVAR_ASSOC) malloc(sizeof(ADDR_CVAR_ASSOC));
		InitializeConditionVariable(&AddrCVarAssocTbl[idxAddrCVarAssoc]->CVar);
		AddrCVarAssocTbl[idxAddrCVarAssoc]->Address = Address;
		AddrCVarAssocTbl[idxAddrCVarAssoc]->NumberOfWaiters = 1;
	} else {
		++AddrCVarAssocTbl[idxAddrCVarAssoc]->NumberOfWaiters;
	}

	ReturnValue = SleepConditionVariableCS(&AddrCVarAssocTbl[idxAddrCVarAssoc]->CVar, &AddrCVarAssocTblLock, dwMilliseconds);
	SleepConditionVariableCSError = GetLastError();

	if (--AddrCVarAssocTbl[idxAddrCVarAssoc]->NumberOfWaiters == 0) {
		free(AddrCVarAssocTbl[idxAddrCVarAssoc]);
		AddrCVarAssocTbl[idxAddrCVarAssoc] = NULL; // important - do not remove
	}
	LeaveCriticalSection(&AddrCVarAssocTblLock);

	SetLastError(SleepConditionVariableCSError);
	return ReturnValue;
}

void WINAPI WakeByAddressAll(
	_In_ PVOID Address)
{
	SIZE_T idxAddrCVarAssoc;
	// look for first slot in the table which contains the matching Address
	EnterCriticalSection(&AddrCVarAssocTblLock);
	for (idxAddrCVarAssoc = 0; idxAddrCVarAssoc < AddrCVarAssocTblSize; ++idxAddrCVarAssoc) {
		if (AddrCVarAssocTbl[idxAddrCVarAssoc] && AddrCVarAssocTbl[idxAddrCVarAssoc]->Address == Address) {
			WakeAllConditionVariable(&AddrCVarAssocTbl[idxAddrCVarAssoc]->CVar);
			break;
		}
	}
	LeaveCriticalSection(&AddrCVarAssocTblLock);
	return;
}

void WINAPI WakeByAddressSingle(
	_In_ PVOID Address)
{
	SIZE_T idxAddrCVarAssoc;
	// look for first slot in the table which contains the matching Address
	EnterCriticalSection(&AddrCVarAssocTblLock);
	for (idxAddrCVarAssoc = 0; idxAddrCVarAssoc < AddrCVarAssocTblSize; ++idxAddrCVarAssoc) {
		if (AddrCVarAssocTbl[idxAddrCVarAssoc] && AddrCVarAssocTbl[idxAddrCVarAssoc]->Address == Address) {
			WakeConditionVariable(&AddrCVarAssocTbl[idxAddrCVarAssoc]->CVar);
			break;
		}
	}
	LeaveCriticalSection(&AddrCVarAssocTblLock);
	return;
}

#pragma comment(linker, "/export:WaitOnAddress")
#pragma comment(linker, "/export:WakeByAddressAll")
#pragma comment(linker, "/export:WakeByAddressSingle")

#pragma comment(linker, "/export:CancelWaitableTimer=kernel32.CancelWaitableTimer")
#pragma comment(linker, "/export:CreateEventW=kernel32.CreateEventW")
#pragma comment(linker, "/export:EnterCriticalSection=kernel32.EnterCriticalSection")
#pragma comment(linker, "/export:InitializeConditionVariable=kernel32.InitializeConditionVariable")
#pragma comment(linker, "/export:InitializeCriticalSection=kernel32.InitializeCriticalSection")
#pragma comment(linker, "/export:InitializeCriticalSectionEx=kernel32.InitializeCriticalSectionEx")
#pragma comment(linker, "/export:LeaveCriticalSection=kernel32.LeaveCriticalSection")
#pragma comment(linker, "/export:ResetEvent=kernel32.ResetEvent")
#pragma comment(linker, "/export:SetEvent=kernel32.SetEvent")
#pragma comment(linker, "/export:SetWaitableTimer=kernel32.SetWaitableTimer")
#pragma comment(linker, "/export:Sleep=kernel32.Sleep")
#pragma comment(linker, "/export:SleepConditionVariableCS=kernel32.SleepConditionVariableCS")
#pragma comment(linker, "/export:TryEnterCriticalSection=kernel32.TryEnterCriticalSection")
#pragma comment(linker, "/export:WakeAllConditionVariable=kernel32.WakeAllConditionVariable")
#pragma comment(linker, "/export:ReleaseSemaphore=kernel32.ReleaseSemaphore")
#pragma comment(linker, "/export:WaitForMultipleObjectsEx=kernel32.WaitForMultipleObjectsEx")
#pragma comment(linker, "/export:WaitForSingleObject=kernel32.WaitForSingleObject")
#pragma comment(linker, "/export:WaitForSingleObjectEx=kernel32.WaitForSingleObjectEx")
