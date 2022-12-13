#写一个堆排序，不调用api,对一个int数组s进行排序

def heap_sort(s):
    #先构建一个大顶堆
    for i in range(len(s)):
        #从最后一个非叶子节点开始，从下往上，从右往左调整
        adjust_heap(s,i)
    #调整完毕，开始排序
    for i in range(len(s)-1,0,-1):
        #把堆顶和最后一个元素交换
        s[0],s[i]=s[i],s[0]
        #调整堆
        adjust_heap(s,i)
    print(s)

def adjust_heap(s,i):
    #从下往上，从右往左调整
    #i为当前需要调整的节点
    #i为当前需要调整的节点
    while i>0:
        #如果当前节点大于父节点，交换
        if s[i]>s[(i-1)//2]:
            s[i],s[(i-1)//2]=s[(i-1)//2],s[i]
        #当前节点变为父节点
        i=(i-1)//2

s=[1,2,3,5,4,6,7,8,9,10]
heap_sort(s)







