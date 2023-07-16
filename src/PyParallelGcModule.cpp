
#include "vector"
#include "GcModuleCommon.h"
#include "GcModuleParentImp.h"
#include "GcModuleChildImp.h"
#include "errno.h"
#include "wait.h"

#define ADD_INT(NAME) if (PyModule_AddIntConstant(m, #NAME, NAME) < 0) return NULL

GHashTable *kGarbageHashTable = NULL;
PyObject *kSaveAllGarbageDict = NULL;
enum GcStatus kGlobalGcStatus = GcStatusUninit;
enum GcDebugFlag KGlobalGcDebugFlag = GcDebugFlagNone;
int kPipeFd[2];
pid_t childPid = 0;

void HandleGcStatusInit(){
    if(-1 == pipe(kPipeFd)){
        printf("failed errno = %d\n", errno);
        PyErr_SetString(PyExc_RuntimeError, "pipe failed!");
        return;
    }

    PyPrepareBeforeCollect();
    childPid = fork();
    if(-1 == childPid){
        printf("fork failed errno = %d\n", errno);
        PyErr_SetString(PyExc_RuntimeError, "fork failed!");
        return;
    }else if(0 == childPid){
        // child
        kGlobalGcStatus = GcStatusChildCollecting;
        DoCollectGarbage();
    }else{
        kGlobalGcStatus = GcStatusParentWaiting;
    }
}

void HandleGcStatusParentWaiting(unsigned int blockTime){

    fd_set set;
    struct timeval timeout;
    int size = 0;

    timeout.tv_sec = (time_t)(blockTime / 1000.0);
    timeout.tv_usec = (blockTime % 1000) * 1000;

    FD_ZERO(&set);
    FD_SET(kPipeFd[0], &set);
    int result = select(kPipeFd[0] + 1, &set, NULL, NULL, &timeout);

    if(result == -1){
        char err[100];
        sprintf(err, "select return error: %d", errno);
        PyErr_SetString(PyExc_RuntimeError, err);
        return;
    }else if(result == 0) {
        //timeout
    }else{
        result = read(kPipeFd[0], (void *)&size, 4);
        if(result < 0){
            char err[100];
            sprintf(err, "read error: %d", errno);
            PyErr_SetString(PyExc_RuntimeError, err);
            return;
        }

        int byteSize = size * sizeof(void *); 
        char *garbageAddress = new char[byteSize];
        int current = 0;

        while(current < byteSize){
            result = read(kPipeFd[0], garbageAddress + current, byteSize - current);
            if(result < 0){
                char err[100];
                sprintf(err, "read kGarbageAddress: %d", errno);
                PyErr_SetString(PyExc_RuntimeError, err);
                delete[] garbageAddress;
                return;
            }
            current += result;
        }
    
        for(int i = 0; i < size; ++i){
            g_hash_table_insert(kGarbageHashTable, *(void **)(garbageAddress + i * sizeof(void *)), NULL);
        }
        
        printf("[Debug] parents read %d garbages \n", g_hash_table_size(kGarbageHashTable));
        
        close(kPipeFd[0]);
        close(kPipeFd[1]);
        wait(&childPid);
        delete[] garbageAddress;
        kGlobalGcStatus = GcStatusCleaning;
    }
}

PyObject *Collect(PyObject *self, PyObject *args){

    unsigned int blockTime = 0;
    if(!PyArg_ParseTuple(args, "I", &blockTime)){
        PyErr_SetString(PyExc_RuntimeError, "collect need 1 args");
        return NULL;
    }

    printf("Begin Collect Status:%d \n", kGlobalGcStatus);
    struct timeval now, targetTimeval, tmp;
    tmp.tv_sec = blockTime / 1000;
    tmp.tv_usec = (blockTime % 1000) * 1000;


    gettimeofday(&now, NULL);
    timeradd(&now, &tmp, &targetTimeval);

    switch (kGlobalGcStatus) {
        case GcStatusInit:
            HandleGcStatusInit();
            break;
        case GcStatusParentWaiting:
            HandleGcStatusParentWaiting(blockTime);
            break;
        case GcStatusCleaning:
            PyDoCleanGarbageList(&targetTimeval);
            break;
        default:
            break;
    }
    gettimeofday(&tmp, NULL);
    timersub(&tmp, &now, &tmp);
    printf("End Collect Status:%d, use %d ms\n", kGlobalGcStatus, (int)(tmp.tv_sec * 1000 + tmp.tv_usec / 1000));

    PyObject *o = PyLong_FromLong(kGlobalGcStatus);
    if(!o){
        PyErr_SetString(PyExc_RuntimeError, "Return Value Has Some Error.");
        Py_RETURN_NONE;
    }else{
        return o;
    }
    
}

PyObject* GetGcDebugFlag(PyObject *self, PyObject *_){
    PyObject *o = PyLong_FromLong(KGlobalGcDebugFlag);
    return o;
}

PyObject* SetGcDebugFlag(PyObject *self,PyObject *newFlag){

    if(kGlobalGcStatus != GcStatusInit){
        PyErr_SetString(PyExc_RuntimeError, "SetGcDebugFlag kGlobalGcStatus should equal GcStatusInit");
        Py_RETURN_NONE;
    }

    int flag = 0;
    if(!PyArg_ParseTuple(newFlag, "I", &flag)){
        PyErr_SetString(PyExc_TypeError, "SetGcDebugFlag Args Need Int");
        return NULL;
    }

    PyObject *oldFlag = PyLong_FromLong(KGlobalGcDebugFlag);
    KGlobalGcDebugFlag = (GcDebugFlag)(flag);
    return oldFlag;
}

PyObject* Init(PyObject *self, PyObject *_){
    PyObject *sourceGcModule = PyImport_ImportModule("gc");
    if(sourceGcModule != NULL){
        _Py_IDENTIFIER(disable);
        {PyObject *result = _PyObject_CallMethodId(sourceGcModule, &PyId_disable, NULL); Py_DECREF(result);}
        Py_XDECREF(sourceGcModule);
    }
    Py_RETURN_NONE;
}

// Method def
static PyMethodDef ParallelMethods[] = {
        //{"Prepare", PyPrepareBeforeCollect, METH_VARARGS, "prepare before collect garbage"},
        {"Init", Init, METH_NOARGS, "InitParallelGc"},
        {"Collect", Collect, METH_VARARGS, "collect garbage"},
        {"SetFlag", SetGcDebugFlag, METH_VARARGS, "SetFlag"},
        {"GetFlag", GetGcDebugFlag, METH_NOARGS, "GetFlag"},
        //{"Clean", PyDoCleanGarbageList, METH_VARARGS, "clear the given garbage list"},
        {NULL, NULL, 0, NULL}   /*Sentinel*/
};

// Module def
static struct PyModuleDef ParallelGCModule = {
        PyModuleDef_HEAD_INIT,
        .m_name = "ParallelGC",    /* name of module */
        .m_doc = "Parallel Gc!",   /*module documentation, may be NULL*/
        .m_size = -1,    /*size of per-interpreter state of the module,
                       or -1 if the module keeps state in global variables*/
        ParallelMethods,
};

// Module Init
PyMODINIT_FUNC
PyInit_ParallelGCModule(void){
    PyObject *m = PyModule_Create(&ParallelGCModule);
    if(m == NULL){
        return NULL;
    }
    
    // // 
    kGlobalGcStatus = GcStatusInit;
    KGlobalGcDebugFlag = GcDebugFlagNone;

    kSaveAllGarbageDict = PyDict_New();  PyModule_AddObject(m, "garbage", kSaveAllGarbageDict);

    // Gc Status
    ADD_INT(GcStatusUninit);
    ADD_INT(GcStatusInit);
    ADD_INT(GcStatusParentWaiting);
    ADD_INT(GcStatusChildCollecting);
    ADD_INT(GcStatusCleaning);

    // GcCleaning Status
    ADD_INT(GcStatusCleaningNone);
    ADD_INT(GcStatusCleaningLookupGarbage);
    ADD_INT(GcStatusCleaningMoveLegacyFinalizers);
    ADD_INT(GcStatusCleaningMoveLegacyFinalizerReachable);
    ADD_INT(GcStatusCleaningHandleWeakRefs);
    ADD_INT(GcStatusCleaningFinalizeGarbage);
    ADD_INT(GcStatusCleaningDeleteGarbage);
    ADD_INT(GcStatusCleaningOver);

    // Gc Debug Flag    
    ADD_INT(GcDebugFlagNone);
    ADD_INT(GcDebugFlagPrintDebug);
    ADD_INT(GcDebugFlagSaveAll);
    ADD_INT(GcDebugFlagHandlerWeakrefs);
    
    kGarbageHashTable = g_hash_table_new(NULL, NULL);
    return m;
}
