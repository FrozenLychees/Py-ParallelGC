# Py-ParallelGC
Py-ParallelGC是Python的一个异步式GC的模块，它基于Fork将Python的GC标记阶段迁移到子进程中执行，并在调用时支持自定义卡顿时间

# 使用方式
将编译好的so放置到需要的工程目录中，然后在项目中Import即可
```
import ParallelGCModule  # import 后会自动关闭Python的原生GC

ParallelGCModule.collect(max_execute_time)
do some other ....
ParallelGCModule.collect(max_execute_time)

```

可以参考test中的测试文件来具体使用，模块提供的一些字段和变量可以看具体介绍的中描述

# 编译
1. 编译依赖Python库以及glib库， 修改CMakeLists.txt中 Python 和 glib 库的路径
2. 使用CMAKE编译，或者执行do_make脚本


# 测试
在test目录下有两个测试，一个是单元测试unitTest,另一个稳定性测试是stabilityTest

# 具体介绍
Python原生GC执行一次collect，Python程序需要暂定直到GC全部执行完毕。
而ParallelGCModule的collect一次并不会暂定到GC完全执行完毕，最大暂定时间是由参数给出。所以在一次完整的GC中，需要执行多次collect，每一次返回之后，程序可以继续运行，减少GC给程序带来的卡顿。

ParallelGCModule主要思路是将标记-回收算法的标记部分移动带子进程中进行，父进程可以继续执行程序。
等父进程收到子进程的垃圾对象ID列表后，就可以执行回收部分的算法，将垃圾对象进行清除。

## 标记部分
因为Python原生的回收是使用GC-HEAD进行标记清除的，在子进程中如果还使用这块内存，会导致COW造成进程内存快速上涨。
为了尽可能避免这个事情，稍微改写了该过程，使用申请的dict空间来代替GCHEAD，减少COW造成的影响。

## 回收部分
在fork之前会将回收的对象都移动到快照链表上，在收到子进程的ID列表后，会判断是否存在于快照链表中，避免回收错误的对象。
在确定好垃圾对象后，其他步骤的主要行为和Python原生的进行相似，额外增加了执行时间的判断，以及对于弱引用标记的判断。


collect会返回当前的状态，可通过这个返回的状态来判断是否完成过一次完整的GC，
```
GcStatusUninit = 0,  # 未初始化
GcStatusInit  # 初始化完毕
GcStatusParentWaiting,  # 父进程等待子进程的垃圾列表
GcStatusChildCollecting ,  # 子进程正在执行标记算法
GcStatusCleaning,  # 父进程正在执行回收算法
```

## 清理阶段的状态
在父进程执行回收算法的时候也有对应特定的状态字段
```

GcStatusCleaningNone = 0,  # 暂未开始清理阶段
GcStatusCleaningLookupGarbage,  # 清理阶段-搜索确定垃圾对象
GcStatusCleaningMoveLegacyFinalizers,  # 清理阶段-查找tp_del对象
GcStatusCleaningMoveLegacyFinalizerReachable, # 清理阶段-移除tp_del对象及其能访问的对象
GcStatusCleaningHandleWeakRefs, # 清理阶段-处理弱引用
GcStatusCleaningFinalizeGarbage, # 清理阶段-调用__del__
GcStatusCleaningDeleteGarbage, # 清理阶段- 删除垃圾对象
GcStatusCleaningOver,  # 清理阶段-恢复清理阶段产生的中间状态
```

## FLAG 标记

同时，ParallelGCModule还提供SetFlag和GetFlag来设置和查看GC的一些执行选项，选项如下：
```
    GcDebugFlagNone = 0, # 空
    GcDebugFlagPrintDebug = 1 << 0, # 输出GC过程中的debug日志
    GcDebugFlagSaveAll = 1 << 1, # 保存所有收集到的垃圾到ParallelGCModule.garbage，garbage是一个dict
    GcDebugFlagHandlerWeakrefs = 1 << 2, # 处理弱引用，默认不处理
```
FLAG的设置只能在开启新的一轮GC之前，在GC过程中调用SetFlag是不会生效的。

### GcDebugFlagSaveAll 
和原生gc一样，如果设置了这个选项，那么GC就不会释放这个对象，相当于跳过了GcStatusCleaningDeleteGarbage这个阶段。
这些垃圾对象会被保存在ParallelGCModule.garbage中，需要程序自己去清理这个垃圾，即需要自己解开循环引用。

这边需要注意的是, 如果在__del__中解开了循环引用，那么可能无法回收到ParallelGCModule.garbage。

### GcDebugFlagHandlerWeakrefs
需要在GC中处理有弱引用的对象，默认情况下是会跳过持有弱引用的对象，原因是在异步情况下，弱引用可以使得垃圾对象复活，
而这种复活在父进程异步回收过程中是无法判断的。所以默认不处理这些有弱引用的对象。

这个选项建议在明白程序中是否存在弱引用以及弱引用是否复活对象等情况后使用。
