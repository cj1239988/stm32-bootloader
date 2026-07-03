# STM32 Bootloader

## 项目简介

本项目基于 STM32F407 实现一个串口 Bootloader，用于学习和实现 MCU 在线升级（IAP）流程。

目前按照模块逐步开发，每完成一个功能都会进行一次 Git 提交，并记录开发过程。

---

## 当前开发进度

### 已完成

- USART3 串口驱动初始化
- 串口中断接收
- 串口发送接口
- 串口接收回调机制
- Bootloader 主循环框架
- TLV 通信协议设计
- 数据包状态机
- 数据包解析框架
- 环形缓冲区（ring buffer）
- 串口接收异步处理
- UART 中断与数据包解析解耦

---

## 通信协议

### 数据包格式

| 字段 | 长度 |
|------|------|
| Header | 1 Byte |
| Opcode | 1 Byte |
| Length | 2 Bytes |
| Payload | n Bytes |

支持的命令：

- ERASE（擦除 Flash）
- PROGRAM（写 Flash）
- VERIFY（CRC 校验）
- BOOT（跳转 APP）

---

## 后续开发计划

- [ ] Flash 擦除
- [ ] Flash 写入
- [ ] CRC 校验
- [ ] Boot 跳转 APP
- [ ] 完整固件升级流程

---

## 开发记录

### 第一次提交

完成 UART Bootloader 基础框架：

- USART3 驱动
- 串口收发接口
- 回调函数机制
- TLV 协议定义
- 数据包状态机
- Bootloader 主循环

### 第二次提交

完成 UART 接收异步优化：

- 新增 ring buffer 环形缓冲区
- USART 接收中断中只负责快速缓存数据
- 主循环从 ring buffer 中逐字节取出数据
- 数据包状态机改为在主循环中处理
- 避免在中断中 printf 导致后续串口数据丢失

后续所有功能均在 dev 分支开发，测试完成后合并至 main 分支。
