# -*- coding: UTF-8 -*- 
import random

class BaseCase(object):

    def __init__(self):
        pass

    def create(self):
        raise RuntimeError("Base create")
    
    def check(self):
        raise RuntimeError("Base check")

    def clean(self):
        raise RuntimeError("Base clean")

execute_del_cnt = 0
class CustomObjHasDel(object):

    def __init__(self):
        self.subobj = []
        pass

    def __iter__(self):
        return iter(self.subobj)

    def __del__(self):
        # print("--del--", "CustomObjHasDel del", id(self))
        # self.subobj.clear()
        global execute_del_cnt
        execute_del_cnt += 1
        pass
    
    def append(self, other):
        self.subobj.append(other)
        pass
    
    def clear(self):
        self.subobj.clear()

# 简单分析当前产生的循环引用个数
def findGarbage(obj_list):

    garbage = {}
    final_garbage = {}
        
    # 遍历OBJ对象，确定存在循环引用
    def _visit_obj(obj):
        queue = [obj]
        tmp_visit = {}
        while queue:
            obj = queue.pop()
            _id = id(obj)
            if _id not in tmp_visit:
                tmp_visit[_id] = obj
            else:
                return obj
            it = iter(obj.values()) if type(obj) is dict else iter(obj)
            for subobj in it:
                queue.append(subobj)
        return None

    # 遍历存在循环引用的对象，这些对象未来一定是垃圾对象
    def _visit_garbage(obj):
        tmp = [obj]
        while tmp:
            obj = tmp.pop()
            _id = id(obj)

            if _id not in final_garbage:
                final_garbage[_id] = obj
                it = iter(obj.values()) if type(obj) is dict else iter(obj)
                for subobj in it:
                    tmp.append(subobj)
        return

    for obj in obj_list:
        if id(obj) in garbage:
            continue
        ret = _visit_obj(obj)
        if ret: 
            _visit_garbage(obj)

    # 开始计算数量，类对象内含一些容器对象，所以算3个
    for obj in list(final_garbage.values()):
        if type(obj) in [CustomObjHasDel]:
            final_garbage[id(obj.__dict__)] = obj.__dict__
            final_garbage[id(obj.subobj)] = obj.subobj
    
    return final_garbage
