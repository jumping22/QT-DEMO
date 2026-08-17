# RK3568 UART 收发测试

在 RK3568 上对 `/dev/ttyS9` 做周期性发收测试：每 100ms 发送 250 字节（含时间戳），同时接收串口数据，并将收发内容分别写入日志文件。

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

```bash
# 默认: /dev/ttyS9, 115200, uart_tx.log / uart_rx.log
./uart_loop_test

# 自定义参数
./uart_loop_test -d /dev/ttyS9 -b 115200 -t uart_tx.log -r uart_rx.log

# 只发 N 包后退出
./uart_loop_test -n 100
```

需要串口设备访问权限（root 或加入 `dialout` 组）。Ctrl+C 停止。

## 发包格式（250 字节）

| 偏移 | 长度 | 内容 |
|------|------|------|
| 0 | 2 | Magic `0x55 0xAA` |
| 2 | 4 | 序列号（小端） |
| 6 | 8 | `tv_sec`（墙钟秒） |
| 14 | 8 | `tv_nsec`（墙钟纳秒） |
| 22 | 2 | payload 长度 |
| 24 | 226 | 填充模式数据 |

日志为文本：每行带接收/发送墙钟时间与完整 hex。
