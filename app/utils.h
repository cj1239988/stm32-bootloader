#ifndef __UTILS_H__
#define __UTILS_H__

#define offsetof(type, member) ((size_t) &((type *)0)->member)//获取结构体成员相对于结构体首地址的字节偏移，用于按偏移访问成员或计算结构体大小

#endif /* __UTILS_H__ */
