#include "glib.h"
#include "GcModuleChildImp.h"

void UpdateRefs(GHashTable *gcHeadHashTable, PyGC_Head *gcHead)
{
    for(PyGC_Head *gc = gcHead->gc.gc_next; gc != gcHead; gc = gc->gc.gc_next){
        //assert(_PyGCHead_REFS(gc) == GC_REACHABLE);
        PyObject* op = FROM_GC(gc);
        assert(g_hash_table_insert(gcHeadHashTable, (gpointer)(FROM_GC(gc)), GINT_TO_POINTER(Py_REFCNT(op))));
    }
}

int VisitDecref(PyObject *op, void *data){
    assert(op != NULL);
    
    if(PyObject_IS_GC(op)){
        gpointer refcntPointer = NULL;
        gboolean isExists = g_hash_table_lookup_extended((GHashTable*)(data), (gpointer)(op), NULL, &refcntPointer);

        if(isExists){
            int refcnt = (GPOINTER_TO_INT(refcntPointer)) - 1;
            assert(refcnt >= 0);
            g_hash_table_replace((GHashTable*)(data), (gpointer)(op), GINT_TO_POINTER(refcnt));
        }
    }
    return 0;
}

int SubtractRefs(GHashTable *gcHeadHashTable, PyGC_Head *gcHead)
{
    for(PyGC_Head *gc = gcHead->gc.gc_next; gc != gcHead; gc=gc->gc.gc_next) {
        traverseproc traverse = Py_TYPE(FROM_GC(gc))->tp_traverse;
        if(traverse){
            traverse(FROM_GC(gc), &VisitDecref, (void *)gcHeadHashTable);
        }
    }
    return 0;
}

int VisitReachable(PyObject *op, void *unreachableHashTable){

    if(PyObject_IS_GC(op)){
        
        gboolean isExists = g_hash_table_lookup_extended((GHashTable*)(unreachableHashTable), (gpointer)(op), NULL, NULL);
        if(isExists){
            g_hash_table_remove((GHashTable*)(unreachableHashTable), (gpointer)(op));

            traverseproc traverse = Py_TYPE(op)->tp_traverse;
        
            if(traverse){
                traverse(op, &VisitReachable, (void *)unreachableHashTable);
            }
        }
    }
    return 0;
}

void MoveUnreachable(GHashTable *gcHeadHashTable, PyGC_Head *gcHead){
    for(PyGC_Head *gc = gcHead->gc.gc_next; gc != gcHead; gc = gc->gc.gc_next){
        
        PyObject *op = FROM_GC(gc);

        gpointer refcntPointer = NULL;
        gboolean isExists = g_hash_table_lookup_extended(gcHeadHashTable, (gpointer)(op), NULL, &refcntPointer);
        if(!isExists){
            continue;
        }


        // 发现弱引用存在一个目前没想道如何解决的问题 
        // 当使用弱引用使垃圾对象复活的话，目前没想到如何处理
        // 所以这边加上开关，如果开启则处理，否则也移除
        // 这边Reachable的对象分以下情况
        // 1. 本来就不是垃圾对象的
        // 2. 本来是垃圾对象，但却有弱引用存在的，且当前模式不支持回收弱引用
    
        bool weakrefResult = PyType_SUPPORTS_WEAKREFS(Py_TYPE(op));
        if(weakrefResult){
            Py_ssize_t offset = Py_TYPE(op)->tp_weaklistoffset;
            PyWeakReference **weakList =  (PyWeakReference **)((PyObject **)((char *)(op) + offset));
            weakrefResult = ((*weakList) != NULL);
        }
    
        if(refcntPointer == NULL && !weakrefResult || (weakrefResult && (KGlobalGcDebugFlag & GcDebugFlagHandlerWeakrefs))){
            continue;
        }

        g_hash_table_remove((GHashTable*)(gcHeadHashTable), (gpointer)(op));
        traverseproc traverse = Py_TYPE(op)->tp_traverse;
        if(traverse){
            traverse(op, &VisitReachable, (void *)gcHeadHashTable);
        }
    }
}

void MoveToGarbageList(gpointer key, gpointer value, gpointer _){
    write(kPipeFd[1], &key, sizeof(void *));
}

void _collect(){

    GHashTable* gcHeadHashTable = g_hash_table_new(NULL, NULL);
    // 遍历young，复制对象引用计数
    UpdateRefs(gcHeadHashTable, &kWaitCollectGCHead);
    // 遍历young，减少所有对象的引用计数
    SubtractRefs(gcHeadHashTable, &kWaitCollectGCHead);
    // 遍历young，将不可达对象挑出来
    MoveUnreachable(gcHeadHashTable, &kWaitCollectGCHead);

    // 弱引用处理
    

    //理论上还在hash表内的的就是垃圾对象
    int size = g_hash_table_size(gcHeadHashTable);

    if(GcDebugFlagPrintDebug & KGlobalGcDebugFlag){
        printf("[DebugFromChild] result = %d, send %lu bytes\n", size, size * sizeof(void *));
    }
    write(kPipeFd[1], &size, sizeof(size));
    g_hash_table_foreach(gcHeadHashTable, &MoveToGarbageList, NULL);
    g_hash_table_destroy(gcHeadHashTable);    

    if(GcDebugFlagPrintDebug & KGlobalGcDebugFlag){
        printf("[DebugFromChild] child done\n");
    }

    return ;
}


void DoCollectGarbage(){
 
    _collect();
    close(kPipeFd[1]);
    exit(0);
}