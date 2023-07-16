# -*- coding: UTF-8 -*- 
import random, weakref
import common
from common import CustomObjHasDel, findGarbage

AllObjCount = 100000  # 创建的对象个数

class RandomCreate(object):

    def __init__(self) -> None:
        self.objs = []
        return

    def create(self):
        all_objects = random.choices([CustomObjHasDel, list, dict], weights=[200, 200, 200], k=AllObjCount)
        self.objs = [type() for type in all_objects]

        return 0
 
    def check(self, recycle_garbage):
        if len(recycle_garbage) != 0:
            raise RuntimeError("RandomCreate check call_back_from_weak_times is wrong: call_back_from_weak_times %d" %
                               (len(recycle_garbage),) )
        return True

    def clean(self, recycle_garbage):
        self.objs.clear()
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
