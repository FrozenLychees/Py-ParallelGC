#include "GcModuleCommon.h"
#include "objimpl.h"
#include "internal/pystate.h"



void GcListMove(PyGC_Head* node, PyGC_Head* list){
    // unlink from current list
    node->gc.gc_prev->gc.gc_next = node->gc.gc_next;
    node->gc.gc_next->gc.gc_prev = node->gc.gc_prev;

    // relink at end of new list
    node->gc.gc_next = list;
    node->gc.gc_prev = list->gc.gc_prev;
    list->gc.gc_prev->gc.gc_next = node;
    list->gc.gc_prev = node;
}

void GcListMerge(PyGC_Head *from, PyGC_Head* to){

    if(from->gc.gc_next == from){
        return;
    }

    from->gc.gc_next->gc.gc_prev = to->gc.gc_prev;
    from->gc.gc_prev->gc.gc_next = to;

    to->gc.gc_prev->gc.gc_next = from->gc.gc_next;
    to->gc.gc_prev = from->gc.gc_prev;

    GcListInit(from);
}

bool HasLegacyFinalizer(PyObject *op){
    return op->ob_type->tp_del != NULL;
}

bool GcListIsEmpty(PyGC_Head *gc){
    return gc->gc.gc_next == gc;
}

void GcListInit(PyGC_Head* list){
    list->gc.gc_next = list;
    list->gc.gc_prev = list;
}