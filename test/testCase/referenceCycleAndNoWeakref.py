# -*- coding: UTF-8 -*- 
import random, weakref
import common
from common import CustomObjHasDel, findGarbage

AllObjCount = 100000  # 创建的对象个数
ReferenceCycleCnt = int(AllObjCount * 0.5)  # 循环引用的个数
# WeakReferenceCnt = int(AllObjCount * 0.3)  # 弱引用的比例


class RandomCreate(object):

    def __init__(self) -> None:
        self.garbage_ids = set()
        return

    def create(self):
        all_objects = random.choices([CustomObjHasDel], weights=[200], k=AllObjCount)
        all_objects = [type() for type in all_objects]

        for i in range(ReferenceCycleCnt):
            a = random.choice(all_objects)
            b = random.choice(all_objects)
            a.append(b)
            if(a != b):
                b.append(a)

        del a, b

        # 计算最后产生的引用
        garbageDict = findGarbage(all_objects)
        for garbageId in garbageDict:
            self.garbage_ids.add(garbageId)
        

        return len(self.garbage_ids)

    def check(self, recycle_garbage):
        if len(recycle_garbage) != len(self.garbage_ids) :
            raise RuntimeError("RandomCreate check call_back_from_weak_times is wrong: call_back_from_weak_times recycle_garbage is %d, self.garbage_ids %d" %
                               (len(recycle_garbage), len(self.garbage_ids)) )
        for key in recycle_garbage:
            if key not in self.garbage_ids:
                raise RuntimeError("RandomCreate check recycleGarbage id not in garbage_ids")
            self.garbage_ids.remove(key)
        return True

    def clean(self, recycle_garbage):
        self.garbage_ids.clear()
        for k, v in recycle_garbage.items():
            if type(v) is CustomObjHasDel:
                v.clear()
        pass

class FixCreate(object):

    def __init__(self) -> None:
        self.garbage_ids = set()
        return

    def create(self):
        
        a = CustomObjHasDel()
        b = CustomObjHasDel()
        c = CustomObjHasDel()

        a.append(b)
        a.append(c)

        b.append(a)
        b.append(c)

        c.append(a)
        c.append(b)

        print(hex(id(a)))
        print(hex(id(b)))
        print(hex(id(c)))

        return 9

    def check(self, recycle_garbage):
        if(len(recycle_garbage ) != 9):
            raise RuntimeError()
        return True

    def clean(self, recycle_garbage):
        self.garbage_ids.clear()
        for k, v in recycle_garbage.items():
            v.clear()
        pass
