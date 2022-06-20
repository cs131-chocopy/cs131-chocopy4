def qsort(a:[int])->[int]:
    def lesser(a:[int],k:int)->[int]:
        x:[int]=None
        i:int=0
        x=[]
        for i in a:
            if i<k:
                x=x+[i]
        return x
    def greater(a:[int],k:int)->[int]:
        x:[int]=None
        i:int=0
        x=[]
        for i in a:
            if i>k:
                x=x+[i]
        return x
    return qsort(lesser(a,a[0]))+[a[0]]+qsort(greater(a,a[0]))
    
a:[int]=None
i:int=0

a = [2, 1, 6, 7, 10, 5, 4, 3, 8, 9]
a = qsort(a)
for i in a:
    print(i)
