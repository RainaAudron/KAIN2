#define _CRT_SECURE_NO_WARNINGS

#include "CORE.H"
#include "STRMLOAD.H"
#include "LOAD3D.H"
#include "DEBUG.H"
#include "TIMER.H"
#include "MEMPACK.H"
#include "VRAM.H"
#include "VOICEXA.H"
#include "GAMELOOP.H"
#include "FONT.H"

#include <stddef.h>

int gCurDir; // offset 0x800CF4E8

struct _LoadQueueEntry* loadFree; // offset 0x800D2724

int loadFromHead; // offset 0x800CF4E0

struct _LoadQueueEntry* loadHead; // offset 0x800D2728

struct _LoadQueueEntry LoadQueue[40]; // offset 0x800D1D24

struct _LoadQueueEntry* loadTail; // offset 0x800D272C

int numLoads; // offset 0x800D2730

void STREAM_NextLoadFromHead()
{
	loadFromHead = 1;
}


// autogenerated function stub: 
// void /*$ra*/ STREAM_NextLoadCanFail()
void STREAM_NextLoadCanFail()
{ // line 42, offset 0x8005faf8
	/* begin block 1 */
		// Start line: 84
	/* end block 1 */
	// End Line: 85

	/* begin block 2 */
		// Start line: 85
	/* end block 2 */
	// End Line: 86
	UNIMPLEMENTED();
}

void STREAM_NextLoadAsNormal()
{ 
	loadFromHead = 0;
}

void STREAM_InitLoader(char *bigFileName, char *voiceFileName)
{ 
	int i;

#if !defined(_DEBUG) && !defined(__EMSCRIPTEN__) || defined(NO_FILESYSTEM)
	LOAD_InitCdLoader(bigFileName, voiceFileName);
#endif

	loadFree = &LoadQueue[0];
	loadHead = NULL;
	loadTail = NULL;
	numLoads = 0;

	for (i = 38; i >= 0; i--)
	{
		LoadQueue[38 - i].next = &LoadQueue[39 - i];
	}

	LoadQueue[39].next = NULL;
}

void STREAM_RemoveQueueHead()
{
	struct _LoadQueueEntry* entry = loadHead;

	loadHead = entry->next;

	if (loadHead == NULL)
	{
		loadTail = NULL;
	}

	entry->next = loadFree;
	numLoads--;
	loadFree = entry;
}

void STREAM_RemoveQueueEntry(struct _LoadQueueEntry *entry, struct _LoadQueueEntry *prev)
{ 
	if (loadTail == entry)
	{
		loadTail = prev;
	}

	if (prev == NULL)
	{
		loadHead = entry->next;
	}
	else
	{
		prev->next = entry->next;
	}

	entry->next = loadFree;
	loadFree = entry;
	numLoads--;
}

struct _LoadQueueEntry* STREAM_AddQueueEntryToTail()
{ 
	struct _LoadQueueEntry* entry;
	
	entry = loadFree;
	if (entry == NULL)
	{
		DEBUG_FatalError("CD ERROR: too many queue entries\n");
	}

	loadFree = entry->next;
	entry->next = NULL;
	
	if (loadTail != NULL)
	{
		loadTail->next = entry;
	}
	else
	{
		loadHead = entry;
	}

	loadTail = entry;
	numLoads++;

	return entry;
}

struct _LoadQueueEntry* STREAM_AddQueueEntryToHead()
{
	struct _LoadQueueEntry* entry;

	entry = loadFree;

	if (entry == NULL)
	{
		DEBUG_FatalError("CD ERROR: too many queue entries\n");
	}

	loadFree = entry->next;

	if (loadHead == NULL || loadHead->status == 1 || loadHead->status == 5 || loadHead->status == 10 || loadHead->status == 8)
	{
		entry->next = loadHead;
		loadHead = entry;
	}
	else
	{
		entry->next = loadHead->next;
		loadHead->next = entry;
	}

	if (loadTail == NULL)
	{
		loadTail = entry;
	}

	numLoads++;

	return entry;
}

int STREAM_IsCdBusy(long* numberInQueue)//Matching - 98.75%
{ 
	if (numberInQueue != NULL)
	{
		numberInQueue[0] = numLoads;
	}

	return numLoads;
}

int STREAM_PollLoadQueue()
{ 
	struct _LoadQueueEntry* queueEntry;
	long size;
	struct _LoadQueueEntry* newQueue;
	LOAD_ProcessReadQueue();

	if (loadHead != NULL)
	{
		queueEntry = loadHead;

		if (gameTrackerX.debugFlags < 0)
		{
			FONT_Print("%s status %d\n", &queueEntry->loadEntry.fileName[0], queueEntry->status);
		}
		
		if (!(gameTrackerX.gameFlags & 0x800) || (queueEntry->status != 5 && queueEntry->status != 8 && queueEntry->status != 10))
		{
			switch (queueEntry->status)
			{
			case 1:

				queueEntry->endLoadTime = TIMER_GetTimeMS();
				LOAD_NonBlockingReadFile(&queueEntry->loadEntry);

				if (LOAD_ChangeDirectoryFlag() != 0)
				{
					newQueue = STREAM_AddQueueEntryToHead();
					sprintf(&newQueue->loadEntry.fileName[0], "(%d)", queueEntry->loadEntry.dirHash);
					newQueue->loadEntry.fileHash = 0;
					newQueue->status = 10;
					newQueue->loadEntry.dirHash = 0;
				}
				else
				{
					queueEntry->status = 2;

					if (queueEntry->mempackUsed != 0)
					{
						MEMPACK_SetMemoryBeingStreamed((char*)queueEntry->loadEntry.loadAddr);
					}

					if (queueEntry->loadEntry.retPointer != NULL)
					{
						queueEntry->loadEntry.retPointer[0] = queueEntry->loadEntry.loadAddr;
					}
				}

				break;
			case 2:

				if (LOAD_IsFileLoading() == 0)
				{
					queueEntry->endLoadTime = TIMER_GetTimeMS() - queueEntry->endLoadTime;

					if (queueEntry->relocateBinary != 0)
					{
						size = LOAD_RelocBinaryData(queueEntry->loadEntry.loadAddr, queueEntry->loadEntry.loadSize);

						if (queueEntry->mempackUsed != 0)
						{
							MEMPACK_Return((char*)queueEntry->loadEntry.loadAddr, size);
						}

						queueEntry->relocateBinary = 0;
					}

					if (queueEntry->loadEntry.retFunc != NULL)
					{
						queueEntry->status = 7;
					}
					else
					{
						queueEntry->status = 4;
						if (queueEntry->mempackUsed != 0)
						{
							MEMPACK_SetMemoryDoneStreamed((char*)queueEntry->loadEntry.loadAddr);
						}

						STREAM_RemoveQueueHead();
					}
				}
				
				break;
			case 5:

				queueEntry->loadEntry.loadAddr = (int*)LOAD_InitBuffers();
				queueEntry->endLoadTime = TIMER_GetTimeMS();

				LOAD_CD_ReadPartOfFile(&queueEntry->loadEntry);

				if (LOAD_ChangeDirectoryFlag() != 0)
				{
					newQueue = STREAM_AddQueueEntryToHead();
					sprintf(&newQueue->loadEntry.fileName[0], "(%d)", queueEntry->loadEntry.dirHash);
					newQueue->loadEntry.fileHash = 0;
					newQueue->status = 10;
					newQueue->loadEntry.dirHash = queueEntry->loadEntry.dirHash;
					LOAD_CleanUpBuffers();
				}
				else
				{
					queueEntry->status = 6;
					queueEntry->loadEntry.posInFile = 0;
				}

				break;
			case 6:

				if (LOAD_IsFileLoading() == 0)
				{
					queueEntry->endLoadTime = TIMER_GetTimeMS() - queueEntry->endLoadTime;
					queueEntry->status = 4;
					
					STREAM_RemoveQueueHead();
					LOAD_CleanUpBuffers();

					if (queueEntry->loadEntry.retFunc == VRAM_TransferBufferToVram)
					{
						VRAM_LoadReturn(queueEntry->loadEntry.loadAddr, queueEntry->loadEntry.retData, queueEntry->loadEntry.retData2);
					}
				}

				break;
			case 7:
				
				queueEntry->status = 4;
				
				STREAM_RemoveQueueHead();
				
				if (queueEntry->mempackUsed != 0)
				{
					MEMPACK_SetMemoryDoneStreamed((char*)queueEntry->loadEntry.loadAddr);
				}

				STREAM_NextLoadFromHead();

				if (queueEntry->loadEntry.retFunc != NULL)
				{
					typedef void (*retFunc)(void*, void*, void*);
					retFunc returnFunction = (retFunc)queueEntry->loadEntry.retFunc;
					returnFunction(queueEntry->loadEntry.loadAddr, queueEntry->loadEntry.retData, queueEntry->loadEntry.retData2);
				}

				STREAM_NextLoadAsNormal();

				break;
			case 8:
				queueEntry->status = 9;
				VOICEXA_Play(queueEntry->loadEntry.fileHash, 0);
				break;
			case 9:

				if (VOICEXA_IsPlaying() == 0)
				{
					LOAD_InitCdStreamMode();
					STREAM_RemoveQueueHead();
				}
				
				break;
			case 10:

				queueEntry->endLoadTime = TIMER_GetTimeMS();

				if (LOAD_ChangeDirectoryByID(queueEntry->loadEntry.dirHash) == 0)
				{
					DEBUG_FatalError("Could not read directory hash %d\n", queueEntry->loadEntry.dirHash);
				}

				queueEntry->status = 11;

				break;
			case 11:

				if (LOAD_IsFileLoading() == 0)
				{
					queueEntry->endLoadTime = TIMER_GetTimeMS() - queueEntry->endLoadTime;
					STREAM_RemoveQueueHead();
				}

				break;
			case 3:
			case 4:
			default:
				break;
			}
		}
	}

	return numLoads;
}

struct _LoadQueueEntry* STREAM_SetUpQueueEntry(char *fileName, void *retFunc, void *retData, void *retData2, void **retPointer, int fromhead)
{ 
	struct _LoadQueueEntry *currentEntry;

	if (fromhead != 0)
	{
		currentEntry = STREAM_AddQueueEntryToHead();
	}
	else
	{
		currentEntry = STREAM_AddQueueEntryToTail();
	}

	
	strcpy(currentEntry->loadEntry.fileName, fileName);

	currentEntry->loadEntry.fileHash = LOAD_HashName(fileName);

	currentEntry->loadEntry.dirHash = LOAD_GetSearchDirectory();
	currentEntry->loadEntry.posInFile = 0;
	currentEntry->loadEntry.checksumType = 1;

	if (LOAD_GetSearchDirectory() != 0)
	{
		currentEntry->loadEntry.dirHash = LOAD_GetSearchDirectory();
		LOAD_SetSearchDirectory(0);
		currentEntry->loadEntry.retFunc = retFunc;
	}
	else
	{
		currentEntry->loadEntry.dirHash = gCurDir;
		currentEntry->loadEntry.retFunc = retFunc;
	}

	currentEntry->loadEntry.retData = retData;
	currentEntry->loadEntry.retData2 = retData2;
	currentEntry->loadEntry.retPointer = retPointer;
	
	if (retPointer != NULL)
	{
#if defined(GAME_X64)
		((intptr_t*)retPointer)[0] = INVALID_MEM_MAGIC;
#else
		((long*)retPointer)[0] = INVALID_MEM_MAGIC;
#endif
	}

	return currentEntry;
}

void STREAM_QueueNonblockingLoads(char *fileName, unsigned char memType, void *retFunc, void *retData, void *retData2, void **retPointer, long relocateBinary)
{ 
	struct _LoadQueueEntry* currentEntry;
	int fromhead;

	fromhead = loadFromHead;
	loadFromHead = 0;
	currentEntry = STREAM_SetUpQueueEntry(fileName, retFunc, retData, retData2, retPointer, fromhead);
	currentEntry->loadEntry.loadAddr = NULL;
	currentEntry->mempackUsed = 1;
	currentEntry->loadEntry.memType = memType;
	currentEntry->relocateBinary = relocateBinary;

	if (memType == 0)
	{
		currentEntry->status = 5;
	}
	else
	{
		currentEntry->status = 1;
	}
}

void LOAD_LoadToAddress(char *fileName, void* loadAddr, long relocateBinary)
{
	struct _LoadQueueEntry* currentEntry;
	
	currentEntry = STREAM_SetUpQueueEntry(fileName, NULL, NULL, NULL, NULL, 0);
	currentEntry->loadEntry.loadAddr = (int*)loadAddr;
	currentEntry->status = 1;
	currentEntry->relocateBinary = relocateBinary;
	currentEntry->mempackUsed = 0;

	while(STREAM_PollLoadQueue() != 0)
	{
	}
}


// autogenerated function stub: 
// void /*$ra*/ LOAD_NonBlockingBinaryLoad(char *fileName /*$a0*/, void *retFunc /*$t0*/, void *retData /*$t1*/, void *retData2 /*$a3*/, void **retPointer /*stack 16*/, int memType /*stack 20*/)
void LOAD_NonBlockingBinaryLoad(char *fileName, void *retFunc, void *retData, void *retData2, void **retPointer, int memType)
{ // line 498, offset 0x800602a8
	STREAM_QueueNonblockingLoads(fileName, memType, retFunc, retData, retData2, retPointer, 1);
}


// autogenerated function stub: 
// void /*$ra*/ LOAD_NonBlockingFileLoad(char *fileName /*$a0*/, void *retFunc /*$v1*/, void *retData /*$t0*/, void *retData2 /*$a3*/, void **retPointer /*stack 16*/, int memType /*stack 20*/)
void LOAD_NonBlockingFileLoad(char *fileName, void *retFunc, void *retData, void *retData2, void **retPointer, int memType)
{ // line 505, offset 0x800602ec
	STREAM_QueueNonblockingLoads(fileName, memType, retFunc, retData, retData2, retPointer, 0);
}

void LOAD_NonBlockingBufferedLoad(char *fileName, void *retFunc, void *retData, void *retData2)
{
	STREAM_QueueNonblockingLoads(fileName, 0, retFunc, retData, retData2, NULL, 0);
}

int LOAD_IsXAInQueue()
{
	struct _LoadQueueEntry* entry;

	entry = loadHead;

	while (entry != NULL)
	{
		if (entry->status - 8 < 2)
		{
			return 1;
		}

		entry = entry->next;
	}

	return 0;
}

void LOAD_PlayXA(int number)
{
	struct _LoadQueueEntry* currentEntry;

	currentEntry = STREAM_AddQueueEntryToTail();
	
	currentEntry->status = 8;
	
	currentEntry->loadEntry.fileHash = number;

	memcpy(currentEntry->loadEntry.fileName, "voice", sizeof("voice"));
}

long* LOAD_ReadFile(char *fileName, unsigned char memType)
{
	void *loadAddr;
    
	STREAM_QueueNonblockingLoads(fileName, memType, NULL, NULL, NULL, &loadAddr, 0);
	
	while (STREAM_PollLoadQueue() != 0)
	{
	}

	return (long*)loadAddr;
}

void LOAD_ChangeDirectory(char *name)
{
	struct _LoadQueueEntry* currentEntry;

	currentEntry = STREAM_AddQueueEntryToTail();
	gCurDir = LOAD_HashUnit(name);
	currentEntry->loadEntry.dirHash = gCurDir;
	currentEntry->loadEntry.fileHash = 0;
	currentEntry->status = 10;
	sprintf(&currentEntry->loadEntry.fileName[0], "dir %s", name);
}

void LOAD_AbortDirectoryChange(char *name)
{ 
	struct _LoadQueueEntry* entry;
	struct _LoadQueueEntry* prev;
	long hash;

	if (loadHead != NULL)
	{
		hash = LOAD_HashUnit(name);
		prev = loadHead->next;
		entry = prev->next;

		while (entry != NULL)
		{
			if (entry->status == 10 && entry->loadEntry.dirHash == hash)
			{
				STREAM_RemoveQueueEntry(entry, prev);
				break;
			}

			prev = entry;
			entry = prev->next;
		}
	}
}

void LOAD_AbortFileLoad(char *fileName, void *retFunc)
{
	struct _LoadQueueEntry *entry;
	struct _LoadQueueEntry *prev;
	long hash;
	typedef void (*ret)(int*, void*, void*);
	ret returnFunction;//?

	if (loadHead != NULL)
	{
		prev = NULL;
		hash = LOAD_HashName(fileName);

		entry = loadHead;

		while (entry != NULL)
		{
			if (entry->loadEntry.fileHash == hash && prev == NULL)
			{
				LOAD_StopLoad();

				if (entry->status == 6)
				{
					LOAD_CleanUpBuffers();
				}

				returnFunction = (ret)retFunc;
				returnFunction(entry->loadEntry.loadAddr, entry->loadEntry.retData, entry->loadEntry.retData2);
				
				STREAM_RemoveQueueEntry(entry, prev);
			}
			else
			{
				prev = entry;
				entry = entry->next;
			}
		}
	}
}
