# STM32 Bootloader

## 项目简介

本项目基于 STM32F407 实现一个串口 Bootloader，用于学习和实现 MCU 在线升级（IAP）流程。

项目按照功能模块逐步开发，每完成一个阶段都会进行 Git 提交，并记录对应的学习内容。

当前版本在原有串口升级流程基础上增加了 Magic Header 机制，用于描述升级固件的类型、写入地址、长度、版本和 CRC 校验信息。Bootloader 启动 APP 前会先校验 Magic Header 和 APP 固件 CRC，避免错误固件、不完整固件或损坏固件被启动。

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
- 按键驱动与按键状态检测
- LED 驱动与运行状态指示
- 长按按键进入 Bootloader
- 无升级操作时自动跳转 APP
- Bootloader 模式下按键复位
- Magic Header 固件信息描述
- Python 脚本生成 `.xbin` 升级文件
- 上位机解析并校验 Magic Header
- APP 启动前 Magic Header 与固件 CRC 校验

---

## Flash 分区

| 区域 | 地址 | 说明 |
|------|------|------|
| Bootloader | `0x08000000` | Bootloader 程序区域 |
| Magic Header | `0x0800C000` | 固件描述头存放区域 |
| APP | `0x08010000` | 用户应用程序区域 |

Bootloader 启动后会先判断是否需要进入升级模式。如果没有按键强制进入 Bootloader，则会检查 Magic Header 和 APP 固件 CRC。校验通过后跳转 APP，校验失败则停留在 Bootloader 等待重新升级。

---

## Magic Header 结构

Magic Header 用于描述升级包中的固件信息。Python 打包脚本、上位机和 Bootloader 使用同一套结构布局，保证三端解析结果一致。

```c
typedef struct {
    uint32_t magic;          // 魔数，用于标识这是一个有效的魔术头
    uint32_t bitmask;        // 位掩码，用于标识哪些字段是有效的
    uint32_t reserved1[6];   // 保留字段，供将来使用

    uint32_t data_type;      // 类型，根据 type 类型选择固件下载位置
    uint32_t data_offset;    // 固件文件相对于 Magic Header 的偏移地址
    uint32_t data_address;   // 固件实际写入 MCU Flash 的地址
    uint32_t data_length;    // 固件长度
    uint32_t data_crc32;     // 固件 CRC32 校验值
    uint32_t reserved2[11];  // 保留字段，供将来使用

    char version[128];       // 固件版本字符串

    uint32_t reserved3[6];   // 保留字段，供将来使用
    uint32_t this_address;   // 该结构体在 MCU Flash 中的实际地址
    uint32_t this_crc32;     // 该结构体本身的 CRC32 校验值
} magic_header_t;

### INQUERY

INQUERY 命令用于查询 Bootloader 信息。

当前支持以下查询参数：

| 参数 | 数值 | 功能 |
|------|------|------|
| VERSION | `0x00` | 查询 Bootloader 版本 |
| MTU | `0x01` | 查询最大 Payload 长度 |

### ERASE

ERASE 命令用于擦除指定的 Flash 区域。

Payload 格式：

| 字段 | 长度 | 说明 |
|------|------|------|
| Address | 4 Bytes | Flash 起始地址，小端格式 |
| Size | 4 Bytes | 擦除大小，小端格式 |

### PROGRAM

PROGRAM 命令用于向指定的 Flash 地址写入固件数据。

Payload 格式：

| 字段 | 长度 | 说明 |
|------|------|------|
| Address | 4 Bytes | Flash 写入地址，小端格式 |
| Size | 4 Bytes | 本次写入的数据长度，小端格式 |
| Data | n Bytes | 待写入的固件数据 |

固件文件需要根据 Bootloader 支持的最大传输长度拆分成多个数据包，循环发送 PROGRAM 命令，直到整个固件写入完成。

### VERIFY

VERIFY 命令用于校验写入 Flash 的固件是否正确。

Payload 格式：

| 字段 | 长度 | 说明 |
|------|------|------|
| Address | 4 Bytes | 校验起始地址，小端格式 |
| Size | 4 Bytes | 校验长度，小端格式 |
| CRC32 | 4 Bytes | 上位机计算得到的 CRC32，小端格式 |

Bootloader 会重新计算指定 Flash 区域的 CRC32，并与上位机发送的 CRC32 进行比较。

### RESET

RESET 命令用于触发 MCU 软件复位。

该命令主要用于协议测试和调试。实际使用时，用户也可以通过设备按键触发复位。

### BOOT

BOOT 命令用于从 Bootloader 跳转到 APP。

跳转前，Bootloader 会先校验 Magic Header 和 APP 固件 CRC。校验通过后，Bootloader 会关闭相关外设和中断，设置中断向量表偏移地址，然后跳转到 APP 的入口地址执行应用程序。

---

## 错误码

| 错误码 | 数值 | 说明 |
|--------|------|------|
| OK | `0x00` | 操作成功 |
| ERR_OPCODE | `0x01` | Opcode 错误 |
| ERR_OVERFLOW | `0x02` | 数据接收长度溢出 |
| ERR_TIMEOUT | `0x03` | 操作超时 |
| ERR_FORMAT | `0x04` | 数据包格式错误 |
| ERR_VERIFY | `0x05` | 数据校验错误 |
| ERR_PARAM | `0x06` | 参数错误 |
| ERR_UNKNOWN | `0xFF` | 未知异常 |

---

## Bootloader 升级流程

完整升级流程如下：

1. 用户长按升级按键并复位或重新上电。
2. Bootloader 检测按键是否持续按下 3 秒。
3. 按键满足长按条件后，设备进入 Bootloader 升级模式。
4. LED 点亮，表示设备已经进入升级模式。
5. 上位机选择 `.xbin` 升级文件，并解析 Magic Header。
6. 上位机检查 Magic Header 魔数、Header CRC 和 APP 数据长度。
7. 上位机查询 Bootloader 版本。
8. 上位机查询 Bootloader 支持的 MTU。
9. 上位机擦除并写入 Magic Header 到 `0x0800C000`。
10. Header 写入完成后，上位机发送 VERIFY 命令进行 CRC32 校验。
11. 上位机擦除 APP 所在的 Flash 区域。
12. 上位机根据 MTU 将 APP 固件拆分成多个数据块。
13. 上位机循环发送 PROGRAM 命令，将 APP 固件写入 `0x08010000`。
14. APP 写入完成后，上位机发送 VERIFY 命令进行 CRC32 校验。
15. 校验成功后，上位机发送 BOOT 命令启动新 APP。
16. Bootloader 校验 Magic Header 和 APP 固件 CRC，关闭相关外设和中断，并跳转到 APP。

---

## 功能说明

### UART 异步接收

串口接收采用 ring buffer 进行异步处理。

USART 接收中断中只负责快速读取数据并写入 ring buffer，主循环再从 ring buffer 中逐字节取出数据，并交给状态机解析。

这样可以避免在中断中进行复杂解析或使用 `printf` 打印日志，降低串口连续接收时丢失数据的风险。

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

Bootloader 在接收数据包的过程中增加了超时检查。

如果两个字节之间的间隔时间过长，说明当前数据包可能接收异常，状态机会自动复位，避免半包数据一直卡在解析流程中。

### CRC16 数据包校验

请求包和响应包末尾都带有 CRC16 字段。

Bootloader 接收到完整数据包后，会重新计算 CRC16，并与数据包中的 CRC16 进行比较。

如果校验失败，说明数据包可能在传输过程中发生错误，Bootloader 会拒绝处理该数据包。

### 命令处理与响应

数据包解析完成后，Bootloader 会根据 Opcode 将命令分发到对应的处理函数。

当前已支持以下命令：

- 查询 Bootloader 信息
- 擦除 Flash
- 写入 Flash
- 校验固件
- MCU 软件复位
- 跳转 APP

命令执行完成后，Bootloader 会通过响应包返回执行结果，方便上位机判断命令是否执行成功。

### Flash 擦除与写入

Bootloader 增加了 STM32 Flash 操作接口。

当前实现了：

- Flash 解锁
- Flash 加锁
- 按扇区擦除 Flash
- 按 4 字节写入 Flash

在执行擦除和写入前，会检查目标地址和长度是否合法，避免访问 Flash 范围之外的地址。

### Bootloader 区域保护

为了防止升级过程中误擦除或覆盖 Bootloader 自身，代码中增加了 Bootloader 区域保护。

当擦除或写入地址位于 Bootloader 所在区域时，Bootloader 会拒绝执行该操作。

### CRC32 固件校验

VERIFY 命令用于校验 APP 固件是否写入正确。

上位机发送 APP 地址、固件长度和期望 CRC32。Bootloader 根据 Flash 中的数据重新计算 CRC32，并与上位机发送的 CRC32 进行比较。

如果两者一致，说明固件写入正确；如果两者不一致，Bootloader 会返回校验错误。

### Magic Header 校验

Magic Header 用于描述固件类型、写入地址、固件长度、版本号和 CRC32 校验值。

上位机在烧录前会先解析 `.xbin` 文件中的 Magic Header，检查魔数、Header CRC 和 APP 数据长度。检查通过后，才会按照 Header 中记录的地址和长度进行擦除、写入和校验。

Bootloader 启动 APP 前也会读取 `0x0800C000` 处的 Magic Header，先检查魔数是否为 `MAGI`，再计算 Header 自身 CRC32，并与 `this_crc32` 对比。Header 合法后，Bootloader 会继续读取 Header 中记录的 APP 地址、长度和 CRC32，对 APP 区域重新计算 CRC32。

只有 Magic Header 和 APP 固件 CRC 都校验通过，Bootloader 才会跳转 APP。否则 Bootloader 会停留在升级模式，等待上位机重新烧录。

### 跳转 APP

BOOT 命令用于从 Bootloader 跳转到 APP。

跳转前，Bootloader 会进行以下处理：

- 校验 Magic Header 和 APP 固件 CRC
- 关闭 Bootloader 状态指示灯
- 停止 Bootloader 使用的定时器
- 反初始化相关 USART
- 关闭 Bootloader 使用的中断
- 设置 APP 的中断向量表地址
- 跳转到 APP 的入口地址

这样可以尽量避免 Bootloader 使用的外设和中断影响 APP 的运行。

### Bootloader 启动模式选择

MCU 上电或复位后会检测升级按键状态。

- 如果升级按键持续按下 3 秒，则进入 Bootloader 升级模式
- 如果按键未按下或未持续按满 3 秒，则检查 Magic Header 和 APP 固件 CRC
- 如果 APP 合法，则直接跳转到 APP
- 如果 APP 不合法，则停留在 Bootloader 等待升级
- 进入 Bootloader 后会等待按键释放，避免同一次按键操作再次触发复位

通过长按按键进入升级模式，可以避免设备每次启动都停留在 Bootloader 中，也能够降低误操作进入升级模式的概率。

### LED 状态指示

由于实际使用时用户通常无法查看串口调试日志，因此 Bootloader 增加了 LED 状态指示功能。

当前 LED 状态定义如下：

| LED 状态 | 设备状态 |
|----------|----------|
| 熄灭 | 正常启动 APP |
| 点亮 | 已进入 Bootloader，等待上位机升级 |

跳转 APP 前，Bootloader 会关闭状态指示灯。

### 按键复位

设备进入 Bootloader 后，再次按下升级按键会触发 MCU 软件复位。

复位后，Bootloader 会重新检测按键状态：

- 按键未持续按下 3 秒：检查 APP 合法性，合法则启动 APP
- 按键持续按下 3 秒：再次进入 Bootloader

这种方式允许用户在不使用串口调试命令的情况下，通过实体按键重新启动设备。

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

这一阶段主要完成 Bootloader 的通信基础，使 MCU 可以通过串口接收上位机发送的数据包。

### 第二阶段：UART 异步接收优化

遇到的问题：

在串口接收中断里直接解析数据包或打印 `printf` 日志时，处理时间过长，可能导致后续串口数据来不及接收。

优化方式：

- 增加 ring buffer
- 中断中只缓存数据
- 主循环中取出数据并解析
- 将数据包状态机从中断移动到主循环

优化效果：

串口中断的执行时间变短，接收数据更加稳定，也方便在主循环中打印调试日志。

### 第三阶段：CRC16 校验与接收超时

遇到的问题：

串口通信过程中可能出现数据丢失、数据错误或半包数据。如果没有校验和超时处理，Bootloader 可能会一直停留在错误状态。

优化方式：

- 在数据包末尾增加 CRC16
- 接收完整数据包后进行 CRC16 校验
- 增加 UART 接收超时判断
- 超时后自动复位数据包状态机

优化效果：

提高了串口通信的可靠性，也增强了 Bootloader 从异常数据中恢复的能力。

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

这一阶段开始从“能够接收并解析数据包”进入到“能够根据命令执行实际 Bootloader 操作”。

### 第五阶段：按键启动控制与 LED 状态指示

遇到的问题：

Bootloader 完成基本升级功能后，MCU 每次启动都需要确定是进入升级模式，还是直接运行 APP。

同时，实际用户通常无法查看串口调试日志，因此需要使用硬件方式显示设备当前是否处于升级状态。

实现方式：

- 增加通用按键驱动
- 增加通用 LED 驱动
- 使用按键决定是否进入 Bootloader
- 按键持续按下 3 秒后进入升级模式
- 未检测到有效长按时直接跳转 APP
- 进入 Bootloader 后点亮 LED
- 等待启动按键释放，避免重复触发
- Bootloader 模式下再次按键可复位 MCU
- 跳转 APP 前关闭 LED
- 跳转 APP 前关闭 Bootloader 使用的外设和中断

实现效果：

设备正常上电时可以直接运行 APP；需要升级时，用户可以通过长按按键进入 Bootloader，并通过 LED 判断设备是否已经进入升级模式。

升级完成后，用户可以再次按下按键复位设备，使 Bootloader 重新执行启动判断并进入 APP。

### 第六阶段：Magic Header 固件合法性校验

新增内容：

- 增加 Magic Header 结构
- 增加 Python 固件打包脚本
- 生成 `.xbin` 升级文件
- Header 中记录 APP 地址、长度、版本和 CRC32
- 上位机烧录前解析并校验 Magic Header
- Bootloader 启动 APP 前校验 Header 和 APP CRC

实现效果：

升级包具备自描述能力，上位机和 Bootloader 可以根据 Header 判断固件是否合法、应该写入哪里、写入多长以及校验值是多少。即使升级过程中断电或写入不完整，Bootloader 也能通过 CRC 校验发现 APP 无效，并停留在 Bootloader 等待重新升级。

---

## 分支说明

后续所有功能均在 `dev` 分支开发，测试完成后再合并至 `main` 分支。
