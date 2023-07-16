import time, os, sys
import ParallelGCModule, psutil 

MB = 1024 * 1024
otherObjsCnt = 200000

def doTest():
    import testCase.hasWeakButNoHandle as HWBNH
    import testCase.hasWeakAndHandle as HWAH
    import testCase.referenceCycleAndNoWeakref as RCNW
    import testCase.noGarbage as NG
    import random

    # commonFlag = ParallelGCModule.GcDebugFlagSaveAll  | ParallelGCModule.GcDebugFlagHandlerWeakrefs | ParallelGCModule.GcDebugFlagPrintDebug
    commonFlag = ParallelGCModule.GcDebugFlagSaveAll  |  ParallelGCModule.GcDebugFlagHandlerWeakrefs 
    moduleFlag = {
        # HWBNH: ParallelGCModule.GcDebugFlagSaveAll | ParallelGCModule.GcDebugFlagPrintDebug
        HWBNH: ParallelGCModule.GcDebugFlagSaveAll 
    }
    moduleList = [RCNW]

    currentGarbageModule = None
    nextGarbageModule = None  
    nextFlag = ParallelGCModule.GcDebugFlagNone

    def randomCreateModule():
        module = random.choice(moduleList)
        instance = module.RandomCreate()
        instance.create()
        flag = moduleFlag[module] if module in moduleFlag else commonFlag
        return instance, flag

    otherObjs = []

    currentGarbageModule, nextFlag = randomCreateModule()
    ParallelGCModule.SetFlag(nextFlag)
    while 1:
        if currentGarbageModule is None:
            currentGarbageModule = nextGarbageModule
            ParallelGCModule.SetFlag(nextFlag)
            nextGarbageModule = None
            nextFlag = ParallelGCModule.GcDebugFlagNone

        status = ParallelGCModule.Collect(100)

        if status == ParallelGCModule.GcStatusInit:
            currentGarbageModule.check(ParallelGCModule.garbage)
            currentGarbageModule.clean(ParallelGCModule.garbage)
            currentGarbageModule = None
            ParallelGCModule.garbage.clear()

            print("---ok ---")
            pid = os.getpid()
            memory_info = psutil.Process(pid).memory_full_info()
            print("do current pid = %s, status = %s,  uss = %s MB, pss = %s MB, rss = %s MB, " % (pid, status,
                memory_info.uss / MB, memory_info.pss / MB, memory_info.rss / MB,))
            pass

        if nextGarbageModule is None:
            nextGarbageModule, nextFlag = randomCreateModule()
        elif len(otherObjs) < otherObjsCnt:
            create_cnt = random.randint(0, 100)
            for i in range(create_cnt):
                otherObjs.append(random.choice([(), {}]))
            if random.randint(0, 100) < 30:
                for i in range(create_cnt):
                    otherObjs and otherObjs.pop(random.randint(0, len(otherObjs) - 1))
        time.sleep(0.5)
    return

if __name__ == "__main__":

    import gc 
    ParallelGCModule.Init()
    # gc.disable()
    import random, ast  #  python3.7 中发现 gc.disable() 之后执行 import random 会有奇怪的引用
    print("automatic collection is enabled: %s..." % gc.isenabled())
    m = gc.collect(2)  # 先去除模块中的垃圾对象
    print("Before Test Clear %s Garbages" % m)

    doTest()
    pass