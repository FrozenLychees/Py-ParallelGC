
#ifdef __cplusplus
extern "C"{
#endif

#include <stdbool.h>
#include <unistd.h>
#include <sys/time.h>

#include "GcModuleParentImp.h"
#include "GcModuleChildImp.h"

#include "objimpl.h"
#include "internal/pystate.h"

#define ParallelGC_MODULE
#define BatchSize 100

PyGC_Head kWaitCollectGCHead;
PyGC_Head kTmpGcHead; // 临时存储链表，避免异步出现重复计算的问题

unsigned short kWaitCollectGeneration;


enum GcCleaningStatus kGlobalGcCleaningStatus;

PyGC_Head unreachable;
PyGC_Head finalizers;
PyGC_Head weakRefs;

struct _Generation
{
    int threshold;
    int count;
};

static struct _Generation __generation[NUM_GENERATIONS] = {
    {0, 0},
    {10, 0},
    {10, 0},
};

bool CheckTimeout(struct timeval* targetTimeval){

    struct timeval now;
    gettimeofday(&now, NULL);
    if(timercmp(&now, targetTimeval, >)){
        return true;
    }
    return false;
}

void PyPrepareBeforeCollect(){

    if(kGlobalGcStatus != GcStatusInit){
        char err[100];
        sprintf(err, "Gc Status is wrong, gcStatus = %d", kGlobalGcStatus);
        PyErr_SetString(PyExc_RuntimeError, err);
        return;
    }

    GcListInit(&kWaitCollectGCHead);
    GcListInit(&kTmpGcHead);
    kGlobalGcCleaningStatus = GcStatusCleaningNone;

    for(int i = NUM_GENERATIONS - 1; i >= 0; i --){
        if(__generation[i].count >= __generation[i].threshold){
            kWaitCollectGeneration = i;
            __generation[i].count = 0;
            break;
        }
    }

    // 父进程首先将需要检测的一系列对象都移动到waitCollectGeneration中，这样之后父进程继续运行也不会影响到当前检测的对象集合
    for(int i = 0; i < kWaitCollectGeneration; ++ i){
        GcListMerge(GEN_HEAD(i), GEN_HEAD(kWaitCollectGeneration));
    }

    GcListMerge(GEN_HEAD(kWaitCollectGeneration), &kWaitCollectGCHead);

    return;
}

void PrintRemainGarbage(gpointer key, gpointer value, void* _){
    printf("[Debug]remain garbage = %p \n", *(void **)key);
}

void GetUnreachableSafely(struct timeval *targetTimeval, PyGC_Head *old){
    int count = 0;

    while(!GcListIsEmpty(&kWaitCollectGCHead)){
        if(++count % BatchSize == 0 && CheckTimeout(targetTimeval))
            return ;

        PyGC_Head *gc = kWaitCollectGCHead.gc.gc_next;
        PyObject *op = FROM_GC(gc);

        //PyObject *opID = PyLong_FromVoidPtr(op);
        if(g_hash_table_contains(kGarbageHashTable, op)){
            _PyGCHead_SET_REFS(AS_GC(op), _PyGC_REFS_TENTATIVELY_UNREACHABLE);
            GcListMove(AS_GC(op), &unreachable);
            g_hash_table_remove(kGarbageHashTable, op);
        }else{
            GcListMove(gc, &kTmpGcHead);
        }
    }
    kGlobalGcCleaningStatus = GcStatusCleaningMoveLegacyFinalizers;
    GcListMerge(&kTmpGcHead,  &kWaitCollectGCHead);
    assert(GcListIsEmpty(&kTmpGcHead));

    if(0 != g_hash_table_size(kGarbageHashTable) && (GcDebugFlagPrintDebug & KGlobalGcDebugFlag)){
        //has garbage did not find in yong;
        printf("[Debug]there has %d garbage did not find in yong;\n",  g_hash_table_size(kGarbageHashTable));
        g_hash_table_foreach(kGarbageHashTable, &PrintRemainGarbage, NULL);
    }

    //clean 
    g_hash_table_remove_all(kGarbageHashTable);
    GcListMerge(&kWaitCollectGCHead, old);

    assert(GcListIsEmpty(&kWaitCollectGCHead));
    assert(g_hash_table_size(kGarbageHashTable) == 0);
    return ;
}

void MoveLegacyFinalizers(struct timeval *targetTimeval){
    int count = 0;


    while(!GcListIsEmpty(&unreachable)){
        if(++count % BatchSize == 0 && CheckTimeout(targetTimeval))
            return;

        PyGC_Head *gc = unreachable.gc.gc_next;
        assert(IS_TENTATIVELY_UNREACHABLE(FROM_GC(gc)));
        if(HasLegacyFinalizer(FROM_GC(gc))){
            GcListMove(gc, &finalizers);
            _PyGCHead_SET_REFS(gc, _PyGC_REFS_REACHABLE);
        }else{
            GcListMove(gc, &kTmpGcHead);
        }
    }

    kGlobalGcCleaningStatus = GcStatusCleaningMoveLegacyFinalizerReachable;
    GcListMerge(&kTmpGcHead,  &unreachable);
    assert(GcListIsEmpty(&kTmpGcHead));
}

int VisitMove(PyObject* op, PyGC_Head* tolist){
    if(PyObject_IS_GC(op) && IS_TENTATIVELY_UNREACHABLE(op)){
        PyGC_Head *gc = AS_GC(op);
        GcListMove(gc, tolist);
        _PyGCHead_SET_REFS(gc, _PyGC_REFS_REACHABLE);
    }
    return 0;
}

void MoveLegacyFinalizerReachable(struct timeval* targetTimeval){
    int count = 0;

    while(!GcListIsEmpty(&finalizers)){
        if(++count % BatchSize == 0 && CheckTimeout(targetTimeval))
            return ;

        PyGC_Head *gc = finalizers.gc.gc_next;

        assert(IS_REACHABLE(FROM_GC(gc)));
        PyObject *op = FROM_GC(gc);
        traverseproc travers = Py_TYPE(op)->tp_traverse;
        travers(op, (visitproc)VisitMove, &finalizers);
        GcListMove(gc, &kTmpGcHead);

    }


    kGlobalGcCleaningStatus = GcStatusCleaningHandleWeakRefs;
    GcListMerge(&kTmpGcHead,  &finalizers);
    assert(GcListIsEmpty(&kTmpGcHead));
}

void HandleWeakRefs(struct timeval *targetTimeval, PyGC_Head *old){
    int count = 0;

    while(!GcListIsEmpty(&unreachable)){
        if(++count % BatchSize == 0 && CheckTimeout(targetTimeval))
            return ;

        PyGC_Head *gc = unreachable.gc.gc_next;
        GcListMove(gc, &kTmpGcHead);

        assert(IS_TENTATIVELY_UNREACHABLE(FROM_GC(gc)));
        PyObject *op = FROM_GC(gc);

        if(!PyType_SUPPORTS_WEAKREFS(Py_TYPE(op)))
            continue;
        // 获取对应的弱引用列表
        Py_ssize_t offset = Py_TYPE(op)->tp_weaklistoffset;
        PyWeakReference **weakList =  (PyWeakReference **)((char *)(op) + offset);

        // 发现弱引用存在一个目前没想道如何解决的问题
        // 当使用弱引用使垃圾对象复活的话，目前没想到如何处理
        // 所以这边加上开关，如果开启则子进程可以收集到，否则这边应该一直是空的
        if(!(GcDebugFlagHandlerWeakrefs & KGlobalGcDebugFlag)){
            assert(*weakList == NULL);
            continue;
        }
        
        for(PyWeakReference *weakRef = *weakList; weakRef != NULL; weakRef = *weakList){
            //清理弱引用指向的对象
            assert(weakRef->wr_object == op);
            _PyWeakref_ClearRef(weakRef);
            assert(weakRef->wr_object == Py_None);

            if(NULL == weakRef->wr_callback)
                continue;
            if(IS_TENTATIVELY_UNREACHABLE(weakRef))
                continue;

            // 因为后续要调用对应的弱引用回调，所以这边需要提升引用计数, 后边执行完毕后会削减掉
            Py_INCREF(weakRef);
            GcListMove(AS_GC(weakRef), &weakRefs);
        }
    }

    while(!GcListIsEmpty(&weakRefs)){
        if(++count % BatchSize == 0 && CheckTimeout(targetTimeval))
            return ;

        PyGC_Head *next = weakRefs.gc.gc_next;
        PyObject *op = FROM_GC(next);
        assert(IS_REACHABLE(op)); //check op can reachable

        PyWeakReference *wr = (PyWeakReference *)op;
        PyObject * callback = wr->wr_callback;
        assert(callback != NULL);

        PyObject * result = PyObject_CallFunctionObjArgs(callback, wr, NULL);
        if(result == NULL){
            PyErr_WriteUnraisable(callback);
        }else{
            Py_DecRef(result);
        }

        Py_DecRef(op);
        if(next == weakRefs.gc.gc_next){
            // 弱引用对象还存活，则移动到old里
            GcListMove(next, old);
        }
    }

    kGlobalGcCleaningStatus = GcStatusCleaningFinalizeGarbage;
    GcListMerge(&kTmpGcHead, &unreachable);
    assert(GcListIsEmpty(&kTmpGcHead));
    return ;
}

void FinalizeGarbage(struct timeval *targetTimeval){

    int count = 0;
    while(!GcListIsEmpty(&unreachable)){
        if(++count % BatchSize == 0 && CheckTimeout(targetTimeval))
            return ;

        PyGC_Head *gc = unreachable.gc.gc_next;
        PyObject *op = FROM_GC(gc);

        GcListMove(gc, &kTmpGcHead);  // 这边先移动，如果销毁了， 会自动从tmpHead中清除
        destructor finalize;
        if(!_PyGCHead_FINALIZED(gc) && PyType_HasFeature(Py_TYPE(op), Py_TPFLAGS_HAVE_FINALIZE) &&
            (finalize = Py_TYPE(op)->tp_finalize) != NULL){
            _PyGCHead_SET_FINALIZED(gc, 1);  //  设置GC头中有关FINALIZED的变量，避免销毁多次
            Py_IncRef(op);
            finalize(op);
            Py_DecRef(op);
        }
    }

    kGlobalGcCleaningStatus = GcStatusCleaningDeleteGarbage;
    GcListMerge(&kTmpGcHead, &unreachable);
    assert(GcListIsEmpty(&kTmpGcHead));
    return ;
}

void DeleteGarbage(struct timeval *targetTimeval, PyGC_Head *old){
    int count = 0;
    while(!GcListIsEmpty(&unreachable)){
        if(++count % BatchSize == 0 && CheckTimeout(targetTimeval))
            return ;
    
        PyGC_Head *gc = unreachable.gc.gc_next;
        PyObject* op = FROM_GC(gc);

        if(KGlobalGcDebugFlag & GcDebugFlagSaveAll){
            GcListMove(gc, old);
            PyObject *key = PyLong_FromVoidPtr(op);
            PyDict_SetItem(kSaveAllGarbageDict, key, op);
            Py_DECREF(key);
            continue;
        }
        
        inquiry clear = Py_TYPE(op)->tp_clear;
        if(clear != NULL){
            Py_IncRef(op);
            clear(op);
            Py_DECREF(op);
        }

        if(unreachable.gc.gc_next == gc) {
            GcListMove(gc, &kTmpGcHead);
        }
    }

    if(GcDebugFlagPrintDebug & KGlobalGcDebugFlag){
        int inSeenCount = 0;
        for(PyGC_Head* next = kTmpGcHead.gc.gc_next; next != &kTmpGcHead; next = next->gc.gc_next){
            printf("object is still alive\n");
            //PyObject_Print()
            ++inSeenCount;
        }
        if(inSeenCount){
            printf("has %d in seen, may be wrong \n", inSeenCount);
        }
    }

    kGlobalGcCleaningStatus = GcStatusCleaningOver;
    GcListMerge(&kTmpGcHead, old);
    assert(GcListIsEmpty(&kTmpGcHead));
}

void CleanOver(struct timeval *targetTimeval, PyGC_Head *old){
    // 将finalizers放入到old中

    if(KGlobalGcDebugFlag & GcDebugFlagSaveAll){
        int count = 0;
        while(!GcListIsEmpty(&finalizers)){
            if(++count % BatchSize == 0 && CheckTimeout(targetTimeval)){
                break;
            }

            PyGC_Head *gc = finalizers.gc.gc_next;
            PyDict_SetItem(kSaveAllGarbageDict, PyLong_FromVoidPtr(FROM_GC(gc)), FROM_GC(gc));
            GcListMove(gc, old);
        }
    }else{
        GcListMerge(&finalizers, old);
    }

    kGlobalGcCleaningStatus = GcStatusCleaningNone;
    kGlobalGcStatus = GcStatusInit;
}

void PyDoCleanGarbageList(struct timeval *targetTimeval){
    PyGC_Head *old;
    if(kWaitCollectGeneration >= NUM_GENERATIONS - 1){
        old = GEN_HEAD(NUM_GENERATIONS - 1);
    }else{
        old = GEN_HEAD(kWaitCollectGeneration);
    }

    // 初始化状态
    if(kGlobalGcCleaningStatus == GcStatusCleaningNone){
        GcListInit(&unreachable);
        GcListInit(&finalizers);
        GcListInit(&weakRefs);
        kGlobalGcCleaningStatus = GcStatusCleaningLookupGarbage;
    }

    enum GcCleaningStatus oldCleaningStatus = kGlobalGcCleaningStatus;
    do{
        if(oldCleaningStatus != kGlobalGcCleaningStatus){
            assert(GcListIsEmpty(&kTmpGcHead));
            oldCleaningStatus = kGlobalGcCleaningStatus;
        }
        switch(kGlobalGcCleaningStatus){
            case GcStatusCleaningLookupGarbage:
                GetUnreachableSafely(targetTimeval, old);
                break;
            case GcStatusCleaningMoveLegacyFinalizers:
                // 处理有定义 tp_del 的对象
                MoveLegacyFinalizers(targetTimeval);
                break;
            case GcStatusCleaningMoveLegacyFinalizerReachable:
                MoveLegacyFinalizerReachable(targetTimeval);
                break;
            case GcStatusCleaningHandleWeakRefs:
                // 处理弱引用对象
                HandleWeakRefs(targetTimeval, old);
                break;
            case GcStatusCleaningFinalizeGarbage:
                // 调用tp_finalization
                FinalizeGarbage(targetTimeval);
                break;
            case GcStatusCleaningDeleteGarbage:
                // delete garbage
                DeleteGarbage(targetTimeval, old);
                break;
            case GcStatusCleaningOver:
                CleanOver(targetTimeval, old);
            case GcStatusCleaningNone:
                break;
        }
    }while(oldCleaningStatus != kGlobalGcCleaningStatus && GcStatusCleaningNone != kGlobalGcCleaningStatus);

    printf("Current kGlobalGcCleaningStatus is %d\n", kGlobalGcCleaningStatus);
}

#ifdef __cplusplus
};
#endif