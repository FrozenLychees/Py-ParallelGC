
if __name__ == "__main__":
    
    
    #import time, random
    import gc
    #import random

    gc.set_debug(gc.DEBUG_SAVEALL | gc.get_debug())
    import weakref
    # gc.disable()
    
    
    
    gc.collect(2)  # 先去除模块中的垃圾对象
    print(gc.garbage)
    for o in gc.garbage:
        if type(o) in [list, dict]:
            o.clear()
    gc.garbage.clear()

    print("--=-=-=")
    import time
    gc.collect(2)  # 先去除模块中的垃圾对象
    print(gc.garbage)