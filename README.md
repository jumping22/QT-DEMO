# RK3568 UART 自环测试

在 RK3568 上对 `/dev/ttyS9` 做自环收发测试（TX 直接回到 RX）：每 100ms 发送 250 字节（含时间戳），同时组帧校验接收数据，退出时统计丢包率。收发内容分别写入日志文件。

## 编译

在板子上直接编译：

```bash
make
```

交叉编译（主机需有 aarch64 工具链）：

```bash
make CROSS_COMPILE=aarch64-linux-gnu-
```

## 运行

硬件上将 TX/RX 短接（或外部环回）后：

```bash
# 默认: /dev/ttyS9, 115200, uart_tx.log / uart_rx.log
./uart_loop_test

# 自定义参数
./uart_loop_test -d /dev/ttyS9 -b 115200 -t uart_tx.log -r uart_rx.log

# 只发 N 包后退出并打印丢包率
./uart_loop_test -n 100
```

需要串口设备访问权限（root 或加入 `dialout` 组）。Ctrl+C 停止；退出前会再等约 500ms 收完尾包，然后打印丢包统计。有丢包或坏帧时进程退出码为 2。

## 发包格式（250 字节）

| 偏移 | 长度 | 内容 |
|------|------|------|
| 0 | 2 | Magic `0x55 0xAA` |
| 2 | 4 | 序列号（小端） |
| 6 | 8 | `tv_sec`（墙钟秒） |
| 14 | 8 | `tv_nsec`（墙钟纳秒） |
| 22 | 2 | payload 长度 |
| 24 | 226 | 填充模式数据 |

接收侧按 Magic 组帧，校验 header/payload；日志为文本，含 seq 与完整 hex。
