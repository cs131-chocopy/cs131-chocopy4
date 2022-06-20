def read()->int:
    x:int=0
    i:str=""
    j:int=0
    s:str=""
    s=input()
    for i in s:
        j=0
        while j<10:
            if i==char[j]:
                x=x*10+j
            j=j+1
    return x
char:[str]=None
a:[int]=None
n:int=0
i:int=0
t:int=0
char=["0","1","2","3","4","5","6","7","8","9"]
a=[]
n=read()
i=0
while i<n:
    t=read()
    a=a+[t]
    i=i+1
i=0
while i<n:
    a[i]=a[i]*2
    i=i+1
for i in a:
    print(i)
