# -*- coding: UTF-8 -*- 
import random, weakref
import common
from common import CustomObjHasDel, findGarbage, BaseCase

AllObjCount = 100000  # 创建的对象个数
ReferenceCycleCnt = int(AllObjCount * 0.5)  # 循环引用的个数
WeakReferenceCnt = int(AllObjCount * 0.3)  # 弱引用的比例

call_back_from_weak_times = 0
def weakref_callback(weakobj):
    global call_back_from_weak_times
    # print("call call_back_from_weak_times")
    call_back_from_weak_times += 1
    return


class RandomCreate(BaseCase):

    def __init__(self) -> None:
        self.proxyobj = []
        self.garbageIds = set()
        self.except_cnt = 0
        return

    def create(self):
        all_objects = random.choices([CustomObjHasDel], weights=[200], k=AllObjCount)
        all_objects = [type() for type in all_objects]

        for i in range(ReferenceCycleCnt):
            a = random.choice(all_objects)
            b = random.choice(all_objects)
            a.append(b)
            b.append(a)

        # 计算最后产生的引用
        garbageDict = findGarbage(all_objects)
        for garbageId in garbageDict:
            self.garbageIds.add(garbageId)

        # weakref obj
        print("create weakobj ...")
        choices_list = [obj for obj in garbageDict.values() if isinstance(obj, CustomObjHasDel)]
        weakref_ins_ids = {}
        for i in range(WeakReferenceCnt):
            ins = random.choice(choices_list)
            self.proxyobj.append(weakref.ref(ins, weakref_callback))
            idx = id(ins)
            if idx not in weakref_ins_ids:
                weakref_ins_ids[idx] = 0
            weakref_ins_ids[idx] += 1
        
        self.except_cnt = len(self.garbageIds)
        return self.except_cnt

    def check(self, recycle_garbage):
        if(call_back_from_weak_times != len(self.proxyobj) and self.proxyobj != 0):
            raise RuntimeError("call_back_from_weak_times: %s != %s" % (call_back_from_weak_times, len(self.proxyobj)))

        if(len(recycle_garbage) != self.except_cnt):
            raise RuntimeError("Check Fail len(self.recycle_garbage): %s != self.except_cnt: %s" %
                                (len(recycle_garbage), self.except_cnt))

        for key in recycle_garbage:
            if key not in self.garbageIds:
                raise RuntimeError("RandomCreate check recycle_garbage id not in garbageIds")
            self.garbageIds.remove(key)

        return True

    def clean(self, recycle_garbage):
        global call_back_from_weak_times
        call_back_from_weak_times = 0
        self.proxyobj.clear()
        self.garbageIds.clear()
        self.except_cnt = 0
        
        for k, v in recycle_garbage.items():
            v.clear()

        