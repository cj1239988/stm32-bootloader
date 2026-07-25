#ifndef __UTILS_H__
#define __UTILS_H__

#include "bitops.h"
//静态数组中有多少个元素。
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
//结构体成员相对于结构体起始地址的字节偏移量。
#define offset_of(type, member) ((size_t) &((type *)0)->member)
//通过结构体成员指针获取结构体指针。
#define container_of(ptr, type, member) ({          \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offset_of(type, member)); })

#endif /* __UTILS_H__ */
