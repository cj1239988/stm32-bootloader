# STM32 Bootloader

## 项目简介

本项目基于 STM32F407 实现一个串口 Bootloader，用于学习和实现 MCU 在线升级（IAP）流程。

项目按照功能模块逐步开发，每完成一个阶段都会进行 Git 提交，并记录对应的学习内容。

---

## 当前开发进度

### 已完成

- USART 串口驱动初始化
- 串口中断接收
- 串口发送接口
- 串口接收回调机制
- Bootloader 主循环框架
- TLV 通信协议设计
- 数据包状态机
- 数据包解析框架
- ring buffer 环形缓冲区
- UART 接收异步处理
- UART 中断与数据包解析解耦
- UART 接收超时检查
- CRC16 数据包校验
- Bootloader 响应包封装
- 命令分发与命令处理框架
- Flash 擦除接口
- Flash 写入接口
- CRC32 固件校验
- Bootloader 区域保护
- Reset 命令处理
- Boot 跳转 APP 命令处理

---

## 通信协议

### 请求数据包格式

| 字段 | 长度 | 说明 |
|------|------|------|
| Header | 1 Byte | 固定为 `0xAA` |
| Opcode | 1 Byte | 命令码 |
| Length | 2 Bytes | Payload 长度，小端格式 |
| Payload | n Bytes | 命令参数 |
| CRC16 | 2 Bytes | 对 Header 到 Payload 的 CRC16 校验 |

### 响应数据包格式

| 字段 | 长度 | 说明 |
|------|------|------|
| Header | 1 Byte | 固定为 `0x55` |
| Opcode | 1 Byte | 对应的命令码 |
| Error Code | 1 Byte | 命令执行结果 |
| Length | 2 Bytes | Payload 长度，小端格式 |
| Payload | n Bytes | 响应数据 |
| CRC16 | 2 Bytes | 对 Header 到 Payload 的 CRC16 校验 |

---

## 支持的命令

| 命令 | Opcode | 功能 |
|------|--------|------|
| INQUERY | `0x01` | 查询 Bootloader 信息 |
| ERASE | `0x81` | 擦除 Flash |
| PROGRAM | `0x82` | 写入 Flash |
| VERIFY | `0x83` | CRC32 校验固件 |
| RESET | `0x21` | MCU 软件复位 |
| BOOT | `0x22` | 跳转到 APP |

---

## 功能说明

### UART 异步接收

串口接收采用 ring buffer 进行异步处理。

USART 接收中断中只负责快速读取数据并写入 ring buffer，主循环再从 ring buffer 中逐字节取出数据，并交给状态机解析。

这样可以避免在中断中进行复杂解析或 `printf` 打印日志，降低串口连续接收时丢数据的风险。

### 数据包状态机

Bootloader 使用状态机逐字节解析数据包。

解析流程包括：

- 检查 Header
- 识别 Opcode
- 解析 Length
- 接收 Payload
- 接收 CRC16
- 校验完整数据包

只有 CRC16 校验通过后，才认为数据包有效，并进入命令处理流程。

### 接收超时检查

Bootloader 在接收数据包过程中增加了超时检查。

如果两个字节之间间隔过长，说明当前数据包可能接收异常，状态机会自动重置，避免半包数据一直卡在解析流程中。

### CRC16 数据包校验

请求包和响应包末尾都带有 CRC16 字段。

Bootloader 接收到完整数据包后，会重新计算 CRC16，并与数据包中的 CRC16 进行比较。

如果校验失败，说明数据包可能在传输过程中出错，Bootloader 会拒绝处理该数据包。

### 命令处理与响应

数据包解析完成后，Bootloader 会根据 Opcode 分发到对应的命令处理函数。

当前已支持查询、擦除、写入、校验、复位和跳转命令。

命令执行完成后，Bootloader 会通过响应包返回执行结果，方便上位机判断命令是否成功。

### Flash 擦除与写入

Bootloader 增加了 STM32 Flash 操作接口。

当前实现了：

- Flash 解锁
- Flash 加锁
- 按扇区擦除 Flash
- 按 4 字节写入 Flash

在执行擦除和写入前，会检查目标地址和长度是否合法，避免访问 Flash 范围之外的地址。

### Bootloader 区域保护

为了防止升级过程中误擦除 Bootloader 自身，代码中增加了 Bootloader 区域保护。

当擦除或写入地址落在 Bootloader 所在区域时，Bootloader 会拒绝执行该操作。

### CRC32 固件校验

VERIFY 命令用于校验 APP 固件是否写入正确。

上位机发送 APP 地址、长度和期望 CRC32，Bootloader 根据 Flash 中的数据重新计算 CRC32，并与上位机传入的 CRC32 进行比较。

如果两者一致，说明固件写入正确。

### 跳转 APP

BOOT 命令用于从 Bootloader 跳转到 APP。

跳转前会关闭相关外设和中断，并设置中断向量表偏移地址，然后跳转到 APP 的入口地址执行应用程序。

---

## 学习记录

### 第一阶段：UART Bootloader 基础框架

完成内容：

- USART 串口驱动
- 串口发送接口
- 串口接收中断
- 接收回调函数机制
- Bootloader 主循环
- TLV 数据包格式
- 数据包状态机

这一阶段主要完成 Bootloader 的通信基础，让 MCU 可以通过串口接收上位机发送的数据包。

### 第二阶段：UART 异步接收优化

遇到的问题：

在串口接收中断里直接解析数据包或打印 `printf` 日志时，处理时间过长，可能导致后续串口数据来不及接收。

优化方式：

- 增加 ring buffer
- 中断里只缓存数据
- 主循环中再取出数据解析
- 数据包状态机从中断中移到主循环处理

优化效果：

串口中断变得更短，接收数据更稳定，也方便在主循环中打印调试日志。

### 第三阶段：CRC16 校验与接收超时

遇到的问题：

串口通信过程中可能出现数据丢失、数据错误或半包数据。如果没有校验和超时处理，Bootloader 可能会一直卡在错误状态。

优化方式：

- 数据包末尾增加 CRC16
- 接收完整包后进行 CRC16 校验
- 增加 UART 接收超时判断
- 超时后自动重置状态机

优化效果：

提高了通信可靠性，也增强了异常恢复能力。

### 第四阶段：命令处理与 Flash 操作

新增内容：

- 增加命令分发机制
- 实现 Bootloader 响应包
- 实现 INQUERY 查询命令
- 实现 ERASE 擦除命令
- 实现 PROGRAM 写入命令
- 实现 VERIFY 校验命令
- 实现 RESET 复位命令
- 实现 BOOT 跳转 APP 命令
- 增加 STM32 Flash 擦除和写入接口
- 增加 Bootloader 区域保护

这一阶段开始从“能接收并解析数据包”进入到“能根据命令执行实际 Bootloader 操作”。

---

## 后续开发计划

- [ ] 完善上位机升级工具
- [ ] 完整固件分包发送流程
- [ ] APP 固件合法性检查
- [ ] APP 启动前栈顶地址和复位入口检查
- [ ] Flash 写入对齐和边界处理优化
- [ ] 错误码和响应机制完善
- [ ] 完整 IAP 升级流程测试

---

## 分支说明

后续所有功能均在 `dev` 分支开发，测试完成后再合并至 `main` 分支。
