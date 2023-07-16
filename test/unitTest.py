# -*- coding: UTF-8 -*-

import time
import ParallelGCModule

def doTtestMode(mode_instance, flag):
    print("create reference cycle....")
    cnt = mode_instance.create()
    print("expect garbage cnt = %d" % (cnt), )

    print("ready to start parallel gc...")
    ParallelGCModule.SetFlag(flag)

    while True:
        status = ParallelGCModule.Collect(100)
        print("py gc status = ", status , "parent main threads runing ...")
        print("-" * 50) 
        if status == ParallelGCModule.GcStatusInit:
            break
        time.sleep(1)
    
    print("=" * 100)
    print("gc is done...")
    # check
    mode_instance.check(ParallelGCModule.garbage)

    # clean..
    mode_instance.clean(ParallelGCModule.garbage)
    ParallelGCModule.garbage.clear()
    
    import gc
    gc.set_debug(gc.DEBUG_SAVEALL | gc.get_debug())
    gc.collect(2)
    if gc.garbage:
        raise RuntimeError("clean over but has %s garbages" % len(gc.garbage))
    print("test ok!")
    return True



if __name__ == "__main__":
    import gc 
    ParallelGCModule.Init()
    # gc.disable()
    import random, ast  #  python3.7 中发现 gc.disable() 之后执行 import random 会有奇怪的引用
    print("automatic collection is enabled: %s..." % gc.isenabled())
    m = gc.collect(2)  # 先去除模块中的垃圾对象
    print("Before Test Clear %s Garbages" % m)

    import testCase.hasWeakButNoHandle as HWBNH
    assert(doTtestMode(HWBNH.RandomCreate(),
           ParallelGCModule.GcDebugFlagSaveAll | ParallelGCModule.GcDebugFlagPrintDebug))

    import testCase.hasWeakAndHandle as HWAH
    assert(doTtestMode(HWAH.RandomCreate(),
                        ParallelGCModule.GcDebugFlagSaveAll | ParallelGCModule.GcDebugFlagPrintDebug | ParallelGCModule.GcDebugFlagHandlerWeakrefs))
    
    import testCase.referenceCycleAndNoWeakref as RCNW 
    assert(doTtestMode(RCNW.RandomCreate(),
                        ParallelGCModule.GcDebugFlagSaveAll | ParallelGCModule.GcDebugFlagPrintDebug | ParallelGCModule.GcDebugFlagHandlerWeakrefs))

    import testCase.noGarbage as NG 
    assert(doTtestMode(NG.RandomCreate(),
                        ParallelGCModule.GcDebugFlagSaveAll | ParallelGCModule.GcDebugFlagPrintDebug | ParallelGCModule.GcDebugFlagHandlerWeakrefs))

    pass