a:object = None
b:[object] = None
e:object=None
f:int =1
c:bool =True
d:str= "114514"
def getx(c:[object]) -> object:
    return c[0]
a = [[1,2],[2,3]]
b = [1,None,[1,2]]
print(getx(b))
e=f
print(e)
e=False if True else object()
print(e)

