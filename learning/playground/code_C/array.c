#include <stdio.h>

int get_size(int data[])
{
    return sizeof(data);
}

int main()
{
    int data1[] = {1, 2, 3, 4, 5};
    size_t size1 = sizeof(data1);

    int *data2 = data1;
    size_t size2 = sizeof(data2);

    int size3 = get_size(data1);

    // 20 8 8 
    // size1求的是数组的大小，size2求的是指针的大小，数组当参数传入后函数退化为指针，size3也是求指针的大小
    printf("size1=%zu, size2=%zu, size3=%d.\n", size1, size2, size3);

    return 0;
}