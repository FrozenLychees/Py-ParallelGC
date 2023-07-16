#pragma once
#ifdef __cplusplus
extern "C"{
#endif

#include "GcModuleCommon.h"

//PyObject* PyModuleInit(PyObject* self, PyObject* args);

void PyPrepareBeforeCollect();

void PyDoCleanGarbageList(struct timeval *targetTimeval);


#ifdef __cplusplus
};
#endif
