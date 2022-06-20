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
    if (len(a)<=1):
        return a
    return qsort(lesser(a,a[0]))+[a[0]]+qsort(greater(a,a[0]))
    
a:[int]=None
i:int=0

a= [30, 97, 82, 90, 71, 60, 38, 69, 18, 85, 65, 53, 45, 24, 52, 36, 9, 84, 66, 99, 47, 94, 57, 35, 5, 11, 89, 75, 3, 43, 10, 22, 80, 46, 70, 25, 23, 4, 79, 26, 6, 21, 2, 55, 41, 98, 74, 51, 50, 64, 72, 67, 34, 95, 16, 93, 63, 15, 42, 27, 28, 92, 91, 13, 62, 73, 1, 88, 56, 76, 54, 48, 7, 17, 37, 83, 31, 14, 8, 77, 86, 49, 29, 12, 40, 59, 61, 44, 81, 100, 68, 39, 96, 20, 87, 32, 78, 58, 33, 19]
a = qsort(a)
for i in a:
    print(i)
