#pragma once
#ifdef __cplusplus
extern "C"{
#endif

#include <sys/time.h>
#include "glib.h"
#include <stdbool.h>
#include "Python.h"
#include "object.h"
#include "frameobject.h"


extern PyGC_Head kWaitCollectGCHead;
extern unsigned short kWaitCollectGeneration;

#define FROM_GC(g) ((PyObject *)(((PyGC_Head *)g) + 1))
#define AS_GC(g) ((PyGC_Head *)(g) - 1)

#define IS_REACHABLE(O) (_PyGC_REFS(O) == _PyGC_REFS_REACHABLE)
#define IS_TRACKED(O) (_PyGC_REFS(O) != _PyGC_REFS_UNTRACKED)
#define IS_TENTATIVELY_UNREACHABLE(O) (_PyGC_REFS(O) == _PyGC_REFS_TENTATIVELY_UNREACHABLE)

#define GEN_HEAD(n) (&_PyRuntime.gc.generations[n].head)

bool HasLegacyFinalizer(PyObject *op);

void GcListInit(PyGC_Head* list);
bool GcListIsEmpty(PyGC_Head *gc);
void GcListMove(PyGC_Head* node, PyGC_Head* list);
void GcListMerge(PyGC_Head *from, PyGC_Head* to);

enum GcStatus{
    GcStatusUninit = 0,
    GcStatusInit,
    GcStatusParentWaiting,
    GcStatusChildCollecting ,
    GcStatusCleaning,
};

enum GcCleaningStatus{
    GcStatusCleaningNone = 0,
    GcStatusCleaningLookupGarbage,
    GcStatusCleaningMoveLegacyFinalizers,
    GcStatusCleaningMoveLegacyFinalizerReachable,
    GcStatusCleaningHandleWeakRefs,
    GcStatusCleaningFinalizeGarbage,
    GcStatusCleaningDeleteGarbage,
    GcStatusCleaningOver,
};

enum GcDebugFlag{
    GcDebugFlagNone = 0,
    GcDebugFlagPrintDebug = 1 << 0,
    GcDebugFlagSaveAll = 1 << 1,
    GcDebugFlagHandlerWeakrefs = 1 << 2,
};

extern enum GcStatus kGlobalGcStatus;
extern enum GcDebugFlag KGlobalGcDebugFlag;

extern int kPipeFd[2];

extern GHashTable *kGarbageHashTable;

extern PyObject *kSaveAllGarbageDict;

#ifdef __cplusplus
};
#endif