# Go 语言教程：从语言到前后端

基准版本：Go 1.27.3（2026 年 10 月 20 日，香港时间）。  
路线：语言写细，环境从短；后端用标准库 `net/http`，路由用 chi；数据库练习用 SQLite，并说明换到 PostgreSQL 时改哪里；前端先做服务端模板加 HTMX，再用 Vite 与原生 JavaScript 调用同一套 JSON API。贯穿项目是一个个人任务板。第 25 章里的任务板已用 `go test ./...` 跑通。

本文是一份可以顺着读的教程，不是语法速查表。每章先讲规则，再给能单独保存运行的例子，然后是容易踩的坑和练习。练习答案在文末附录。例子里的 `// runnable` 表示这是一个完整的 `package main` 程序：保存为 `main.go`，在模块目录里执行 `go run .`。

目录

- 第 0 章　Go 在解决什么问题
- 第 1 章　环境
- 第 2 章　程序与包
- 第 3 章　名字、类型与零值
- 第 4 章　复合类型
- 第 5 章　控制流
- 第 6 章　函数
- 第 7 章　方法与接口
- 第 8 章　错误
- 第 9 章　泛型
- 第 10 章　并发
- 第 11 章　模块
- 第 12 章　测试
- 第 13 章　写 Web 时会反复用到的标准库
- 第 14 章　Go 的主要开发方向
- 第 15 章　HTTP
- 第 16 章　服务骨架
- 第 17 章　JSON API
- 第 18 章　数据
- 第 19 章　登录与权限
- 第 20 章　测试、静态文件与镜像
- 第 21 章　页面由 Go 渲染
- 第 22 章　HTMX
- 第 23 章　给浏览器的 API 契约
- 第 24 章　Vite 与原生 JavaScript
- 第 25 章　任务板
- 附录　练习答案

---

## 第 0 章　Go 在解决什么问题

Go 是一门面向并发网络服务的编译型语言。标准库自带 HTTP 服务器、JSON、数据库接口和日志。本教程的服务器一侧全部用 Go 写。浏览器一侧有两种做法，后面都做：由 Go 输出 HTML，或由 JavaScript 调用 Go 的 JSON 接口。

### 0.1 这门语言把注意力放在服务器上

Go 由 Robert Griesemer、Rob Pike、Ken Thompson 在 Google 设计，2009 年公开发布。它要写的程序通常长期运行、同时处理大量网络连接，并且由很多人一起维护。HTTP API、反向代理、命令行工具、服务网格里的组件，都落在这个范围里。

本教程的终点是前后端里的这一条：浏览器是客户端，Go 是服务器。服务器做三件事。

1. 接受连接，读懂 HTTP 请求。
2. 调用业务代码和数据库，得到一个结果或一个错误。
3. 把结果写成 HTML，或写成 JSON。

### 0.2 五条每天都会碰到的设计

**编译成一个自带运行时的程序。** `go build` 产出一个可执行文件。文件里有你的代码，也有 Go 运行时：调度器、垃圾回收、以及用到的标准库。目标机器上需要这个文件，不需要再安装 Go。纯 Go 程序使用 `CGO_ENABLED=0` 构建时，不依赖本机的 C 运行库。第 20 章的镜像按这个方式打包。交叉编译是设置 `GOOS` 和 `GOARCH` 再执行一次 `go build`。前后端主线用不到，知道有这条路即可。

**类型在编译时就定下来。** 变量、参数、结构体字段都有类型。编译器在生成二进制之前拒绝把 `string` 当成 `int` 这类错误。编辑器里的 `gopls` 使用同一套类型信息。任务在教程里写成：

```go
type Task struct {
    ID    int64  `json:"id"`
    Title string `json:"title"`
    Done  bool   `json:"done"`
}
```

反引号里的 `` `json:"id"` `` 是结构体标签。`encoding/json` 用它决定 JSON 的键名。页面模板和 JSON 接口共用这一个类型：业务函数返回 `Task`，外层决定写成 HTML 还是 JSON。

函数内部可以用 `task := Task{Title: "买牛奶"}`。变量的类型仍然是 `Task`，只是不必再写一遍。包的公开 API 保持类型写明。

Go 1.18 起可以写泛型函数和泛型类型。Go 1.27 起，具体类型的方法也可以带自己的类型参数。Web 业务代码里，大多数函数仍然是普通类型。泛型放在第 9 章，用在确实在重复的辅助函数上。

关键字一共 25 个：

```text
break    default     func    interface  select
case     defer       go      map        struct
chan     else        goto    package    switch
const    fallthrough if      range      type
continue for         import  return     var
```

`for` 承担循环，`if` 和 `switch` 承担分支。`gofmt` 统一空格和换行，项目之间的差异主要在结构。泛型使用方括号，没有新增关键字。`any` 是预声明别名，不是关键字。

**内存由运行时回收。** 你用结构体、切片和 `map` 描述数据。对象不再被引用之后，由垃圾回收器收回。业务代码里没有配对的 `free`。请求处理函数里创建的对象跟这次请求走，函数返回后变得不可达，随后被回收。

回收器是并发标记清除，目标是把停顿压到适合服务器的范围。分配本身仍有成本。一个每秒数万次、每次分配大量短命对象的接口，延迟尾部会受分配速度影响。任务板离这个量级很远。

指针仍然存在，用来区分「有没有这个值」、在函数间共享一份结构体、以及让方法修改接收者。指针指向的内存仍由回收器管理。

**并发用 goroutine 来写。** `go` 后面跟一个函数调用，就在一个新的 goroutine 里执行它。goroutine 由运行时调度，初始栈很小，一个进程里可以同时有很多个。操作系统线程少得多：默认大约每个 CPU 核一个工作线程。`net/http` 每接受一条连接，就安排 goroutine 来读这条连接上的请求并调用你的处理函数。同时有 200 个客户端连着，服务器侧就是大约 200 条这样的执行流。你写处理函数时按「这一次请求」来写。

goroutine 之间传递数据有两条常用的路。通道把一个值的所有权交给另一个 goroutine，适合任务队列、超时和取消。`sync.Mutex` 保护一块会被多个 goroutine 读写的内存，适合缓存和计数器。标准库两条路都用：连接池用锁保护空闲连接，请求取消用 `context.Context` 沿着调用链传播。

有一句流传很广的话：不要靠共享内存来通信，要靠通信来共享内存。把它理解成优先考虑「谁拥有这个值」。多个请求同时改同一张内存里的 `map` 时，就用互斥锁，或改由一个 goroutine 独占这张 `map`。数据竞争用 `go test -race` 查，第 10 章会做。

**失败是返回值。** 可能失败的函数返回结果和 `error`。调用方立刻判断。

```go
func LoadTask(ctx context.Context, id int64) (Task, error) {
    row := db.QueryRowContext(ctx,
        `SELECT id, title, done FROM tasks WHERE id = ?`, id)
    var t Task
    if err := row.Scan(&t.ID, &t.Title, &t.Done); err != nil {
        return Task{}, fmt.Errorf("load task %d: %w", id, err)
    }
    return t, nil
}
```

这段是全书反复出现的形状。`%w` 把原来的错误包进去。外层用 `errors.Is` 判断是不是「没有这一行」，用 `errors.As` 取出驱动的具体错误。`panic` 表示这条执行流已经无法按正常返回继续，例如启动时缺了必填配置。业务失败写成 `error`，再在处理函数里变成 400、404 或 500。`net/http` 在调用处理函数时带了恢复逻辑：处理函数里的 panic 会被记进日志，这条连接被关掉，进程继续服务其他连接。

### 0.3 一次请求怎么穿过服务器

浏览器发出 `GET /tasks/17`。

1. 连接已经建立则复用，否则新建一条到监听端口的 TCP 连接。
2. 负责这条连接的 goroutine 解析方法、路径、请求头和正文。
3. 路由按方法和路径找到处理函数。教程先用标准库 `ServeMux`，第 16 章换 chi。
4. 处理函数从路径读出 `17`。读失败就写 400 并返回。
5. 处理函数调用 `LoadTask(r.Context(), 17)`。`r.Context()` 和这次请求绑在一起。用户断开或时限到了，这个 `Context` 会被取消，查询可以跟着停下。
6. 成功得到 `Task`，失败得到 `error`。没有这一行就写 404。查询本身坏了就写 500，日志里留下被 `%w` 包起来的原因。
7. 页面处理函数用 `html/template` 写出 HTML。接口处理函数用 `encoding/json` 写出 JSON。
8. 函数返回。这次请求上的临时对象随后被回收。这条连接可以读下一个请求。

`http.Handler` 是标准库里很小的接口：

```go
type Handler interface {
    ServeHTTP(ResponseWriter, *Request)
}
```

一个类型只要有这个方法，就可以被服务器调用。路由、中间件、测试里的假处理器都从这里接上。

### 0.4 同一套业务，两套界面

```text
浏览器地址栏   GET /tasks/17      页面处理函数  --+
                                                  +-->  存储层  -->  SQLite
浏览器脚本     GET /api/tasks/17  接口处理函数  --+
```

存储层返回 `Task` 与 `error`，它不知道调用方要 HTML 还是 JSON。页面用 Cookie 会话。接口用 `Authorization: Bearer <令牌>`。本教程的令牌是服务端保存的随机会话编号，不是 JWT。检查「这个人能不能看这张任务」的条件写在 SQL 的 `user_id` 里，两种入口共用。

### 0.5 这些选择的成本

二进制包含运行时，所以一个只监听端口的程序也有数十兆体积和一份基础内存。任务板接受这个体积，换来部署时只拷贝一个文件。

`if err != nil` 让源文件变长。读代码时顺着返回值就能看到失败去了哪里。

浏览器端的界面在 Go 之外。第 24 章的 JavaScript 只覆盖列表、表单、登录态和错误提示。

### 0.6 练习

1. 用自己的话说明：200 个客户端同时连着时，进程里 goroutine 和操作系统线程大概各是什么数量级。
2. 打开数据库失败时，函数把什么返回给调用方？调用方接着做什么？
3. 为什么页面和 JSON 接口可以共用读任务的函数？

---

## 第 1 章　环境

这一章结束时，机器上有 Go 1.27.3，有一个能编译的模块，编辑器能跳转到定义。后面的例子都放进模块里跑。

### 1.1 安装

到 [https://go.dev/dl/](https://go.dev/dl/) 下载 Go 1.27.3。

- Windows（64 位）：`go1.27.3.windows-amd64.msi`。安装后重新打开终端。
- macOS（Apple 芯片）：`go1.27.3.darwin-arm64.pkg`。Intel 芯片用 `go1.27.3.darwin-amd64.pkg`。
- Linux（64 位）：

```bash
curl -fsSL -o /tmp/go1.27.3.linux-amd64.tar.gz https://go.dev/dl/go1.27.3.linux-amd64.tar.gz
sudo rm -rf /usr/local/go
sudo tar -C /usr/local -xzf /tmp/go1.27.3.linux-amd64.tar.gz
echo 'export PATH=/usr/local/go/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

确认：

```bash
go version
```

输出里应有 `go1.27.3`。若系统里还有旧的 `go`，`which go` 应指向刚安装的那一个。

国内网络下载模块慢时，可以设置：

```bash
go env -w GOPROXY=https://goproxy.cn,direct
```

香港和能直连 `proxy.golang.org` 的环境保持默认即可。

### 1.2 第一个模块

Go 1.11 之后用模块管理依赖。一个项目一个 `go.mod`，路径不再依赖 `GOPATH`。

```bash
mkdir tasknotes
cd tasknotes
go mod init example.com/tasknotes
```

`example.com/tasknotes` 是模块路径。还没发布到网上也用这种写法。以后被别的项目导入时，导入路径以它开头。

写 `main.go`：

```go
// runnable
package main

import "fmt"

func main() {
    fmt.Println("你好，Go")
}
```

```bash
go run .
```

`go run .` 编译当前目录的包并运行。`go build -o tasknotes .` 在当前目录留下可执行文件。`go fmt ./...` 格式化。`gofmt` 的结果是确定的，提交前跑一次，评审时就不用讨论空格。

### 1.3 五个会反复用的命令

| 命令 | 作用 |
| --- | --- |
| `go run .` | 编译并运行当前包 |
| `go test ./...` | 运行当前模块所有测试 |
| `go fmt ./...` | 按统一格式改写源码 |
| `go vet ./...` | 报告可疑写法，例如格式字符串和参数对不上 |
| `go doc fmt.Println` | 读标准库或当前模块的文档 |

`go env` 打印工具链看到的路径。`go help build` 看某个子命令的说明。

### 1.4 编辑器

Cursor 或 VS Code 安装官方 Go 扩展。它会使用 `gopls`。第一次打开 `.go` 文件时按提示安装工具。确认这三件事：

- 保存时执行 `gofmt`（扩展里的 Format on Save）。
- 按住 Ctrl 或 Cmd 点击 `fmt.Println` 能跳进定义。
- 把 `fmt.Println("你好，Go"` 写成少一个括号，保存前就能看到报错。

`gopls` 读的是磁盘上的模块。`go.mod` 必须在项目根上，源码文件的 `package` 行必须和目录匹配。一个目录里的文件属于同一个包，测试文件用 `package main` 或 `package main_test`。

### 1.5 目录从现在开始长什么样

语言阶段可以一直用这一个模块，每章一个文件：

```text
tasknotes/
  go.mod
  ch03_types.go
  ch04_slices.go
```

同一个 `package main` 里只能有一个 `main`。所以每一章的完整例子要么单独放一个目录，要么练习时先移走别的 `main`。从第 15 章起，任务板使用自己的模块 `example.com/taskboard`，不要和语言练习混在同一个 `go.mod` 里。

### 1.6 练习

1. 安装完成后，`go version` 的输出是什么？
2. `go run .` 和 `go build` 的差别在运行结束后磁盘上留下什么？
3. 把打印改成你的名字，跑 `go fmt`，看它动了哪些空白。

---

## 第 2 章　程序与包

### 2.1 文件、包、模块

模块是一组一起版本化的包，由 `go.mod` 圈定。包是一组一起编译的文件，通常就是一个目录。文件第一行是 `package 名字`。

`package main` 是程序入口。其中必须有 `func main()`。库包用别的名字，习惯上和目录名相同。`internal/store` 的包名写成 `store`。

调用方只能使用大写开头的名字。`Store`、`Open`、`ErrNotFound` 可以被别的包引用。`openTestDB`、`userByEmail` 只在本包内可见。这个规则对函数、类型、字段、方法都成立。测试文件属于同一个包时，可以调用小写名字。外部测试包 `package store_test` 只能看见大写名字，这更接近真实调用方。

### 2.2 导入

```go
import (
    "context"
    "fmt"

    "example.com/taskboard/internal/store"
)
```

标准库放前面，空一行，再写本模块和其他模块。`go fmt` 会分组。导入了却不用，编译失败。只为了执行包的 `init` 而导入时，写成 `_ "modernc.org/sqlite"`。给包起别名写成 `jsonv2 "encoding/json/v2"`。点导入 ` . "fmt"` 会把名字倒进当前文件，教程里不用。

导入路径是模块路径加包目录。`example.com/taskboard/internal/store` 对应这个模块里的 `internal/store` 目录。

### 2.3 internal

路径里有 `internal` 的包，只能被 `internal` 的父目录及其子目录导入。`example.com/taskboard/internal/store` 可以被 `example.com/taskboard/cmd/taskboard` 导入，不能被别的模块导入。任务板把数据库和 HTTP 放进 `internal`，是为了以后改表结构时不必假装这些包是公共 API。

### 2.4 初始化顺序

一个包被导入时，先初始化包级变量，再运行该包的 `init` 函数，然后才回到导入方。依赖先于当前包初始化。一个包可以有多个 `init`，按文件名顺序执行。`init` 没有参数和返回值，不能手动调用。

适合放进 `init` 的东西很少。任务板只用它生成一个 bcrypt 占位哈希，让「查无此邮箱」和「密码错误」花费相近的时间。可以失败的启动步骤放在 `main` 里，失败就打印日志并 `os.Exit(1)`。不要在 `init` 里连接数据库：失败时你还没有日志器，测试也会被迫连上真实服务。

包级变量的初始化表达式不能互相循环引用。`var a = b + 1` 且 `var b = a + 1` 无法编译。

### 2.5 包名怎么读

调用时写包名：`fmt.Println`、`store.CreateTask`。包名宜短、宜小写、宜不要下划线。`util`、`common`、`base` 这种名字会在两个月后失去含义。按它提供的东西命名：`store`、`web`。

### 2.6 练习

1. 为什么 `func open()` 在另一个模块里无法调用？
2. `_ "modernc.org/sqlite"` 的下划线在避免什么编译错误？
3. 把「读取配置文件，读不到就退出」放在 `init` 还是 `main`？说明理由。

---

## 第 3 章　名字、类型与零值

### 3.1 声明

```go
var title string
var count int = 3
title := "买牛奶"
const defaultPort = 8080
```

`var` 可以写在函数外。`:=` 只能写在函数内，至少有一个变量是新的。`count, err := f()` 在 `err` 已存在、`count` 是新名字时合法。如果两边都已存在，必须用 `=`。

常量在编译期确定。无类型常量可以保持高精度，赋给变量时才变成具体类型：

```go
const pi = 3.14159265358979323846
var x float32 = pi
var y float64 = pi
```

`iota` 在一组常量里从 0 递增。适合状态码这类相关整数，不适合会写进数据库、以后要保持稳定含义的业务状态。写进数据库的状态用明确的字符串 `"open"`、`"done"`，读代码的人不用回头查 iota 的顺序。

```go
type Level int

const (
    Debug Level = iota
    Info
    Warn
    Error
)
```

这里 `Debug` 是 0，`Error` 是 3。若在中间插入一行，后面的值都会变。这就是它不适合持久化的原因。

### 3.2 零值

声明了但没有赋值的变量是零值，不是未定义。

| 类型 | 零值 |
| --- | --- |
| 数值 | `0` |
| `bool` | `false` |
| `string` | `""` |
| 指针、切片、`map`、函数、接口、通道 | `nil` |
| 结构体 | 每个字段的零值 |

零值让 `var t Task` 可以直接用：`t.Done` 是 `false`，`t.Title` 是空字符串。返回失败时写 `return Task{}, err`，调用方在 `err != nil` 时不应读取那个 `Task`。

### 3.3 基本类型

整数有 `int8`、`int16`、`int32`、`int64` 和它们的无符号对应物，以及跟机器字长相关的 `int`、`uint`、`uintptr`。任务编号用 `int64`。不要用 `int` 表示会写进数据库或 JSON 的标识，因为 32 位平台上的 `int` 只有 32 位。

`byte` 是 `uint8` 的别名。`rune` 是 `int32` 的别名，表示一个 Unicode 码点。

浮点有 `float32` 和 `float64`。金额不要用浮点。任务板不涉及金额。复数 `complex64`、`complex128` 在 Web 业务里很少出现。

不同类型的数值不能直接运算，必须转换：`int64(count)`。转换可能截断。把 `int64` 转成 `int32` 时，超出范围的高位被丢掉，编译器不会替你检查。

`bool` 不是整数。`if count` 这种写法无法编译，要写 `if count != 0`。

### 3.4 字符串、字节、rune

字符串是不可变的字节序列，约定为 UTF-8。中文「买」在 UTF-8 里占 3 个字节。`len("买")` 是 3，不是 1。`len([]rune("买牛奶"))` 才是字符数。任务标题的 200 字上限按 rune 计，第 18 章的 `NormalizeTitle` 就是这样做的。

```go
// runnable
package main

import (
    "fmt"
    "unicode/utf8"
)

func main() {
    s := "买牛奶"
    fmt.Println(len(s), utf8.RuneCountInString(s))
    for i, r := range s {
        fmt.Printf("字节下标 %d 字符 %c\n", i, r)
    }
    fmt.Println(s[0]) // 第一个字节，不是第一个字
}
```

`range` 字符串时，下标按字节跳，值的类型是 `rune`。按下标切 `s[0:1]` 可能切在一个汉字的中间，得到非法 UTF-8。按字符切之前先转成 `[]rune`，处理完再转回 `string`。转换会分配新内存。

原始字符串用反引号，内容可以跨行，不处理转义。SQL 和 HTML 片段适合用它。反引号字符串里不能再包含反引号。

`strings.Builder` 用来拼接大量片段。循环里用 `+` 拼接长字符串会反复分配。

Go 1.27 的 `strings.CutLast("a/b/c", "/")` 返回 `"a/b"`、`"c"`、`true`。它从最后一次出现的分隔符切开。`Cut`、`CutPrefix`、`CutSuffix` 在更早的版本里就有。

### 3.5 练习

1. `var n int` 和 `n := 0` 的结果值一样。它们在声明位置上有什么差别？
2. 为什么标题长度不用 `len(title) > 200`？
3. 下面的常量若写进数据库，三个月后在 `Warn` 前插入 `Notice` 会发生什么？

```go
const (
    Debug = iota
    Info
    Warn
)
```

---

## 第 4 章　复合类型

### 4.1 数组与切片

数组长度是类型的一部分。`[3]int` 和 `[4]int` 是不同类型。数组赋值会复制全部元素。Web 代码里很少直接用数组，固定长度的令牌缓冲可以用 `[32]byte`。

切片是对一段数组的视图。它内部有指针、长度 `len` 和容量 `cap`。`len` 是当前元素个数，`cap` 是从切片起始位置到背后数组末尾还能容纳多少。

```go
// runnable
package main

import "fmt"

func main() {
    a := []int{1, 2, 3, 4}
    b := a[:2]
    b = append(b, 9)
    fmt.Println("a", a, "b", b)

    c := a[:2:2]
    c = append(c, 8)
    fmt.Println("a", a, "c", c)
}
```

第一次 `append` 时 `b` 的容量还够，新元素写进 `a` 背后的数组，`a` 变成 `[1 2 9 4]`。三下标切片 `a[:2:2]` 把容量限制成 2，再 `append` 会分配新数组，`a` 不再被改写。函数若把切片返回给调用方，又在里面继续 `append`，就要确认容量是否独立。不确定时用三下标，或 `slices.Clone`。

`append` 在容量不够时分配新数组，通常按大约两倍增长，并把旧元素复制过去。所以 `append` 的结果必须接住：`s = append(s, x)`。丢掉返回值，可能把元素写进了调用方看不见的新数组。

`nil` 切片的长度和容量都是 0，可以 `append`，可以 `range`。它和 `[]int{}` 在 JSON 里不同：`nil` 切片编码成 `null`，空切片编码成 `[]`。API 返回列表时，没有数据就返回空切片，避免客户端把 `null` 和 `[]` 分成两条路。任务板的列表接口在返回前做了这个替换。

`copy(dst, src)` 按较短的那个长度复制。它不会帮你扩容。

### 4.2 map

`map[string]int` 的零值是 `nil`。读 `nil` map 得到零值。写入 `nil` map 会 panic。先 `make(map[string]int)` 或写成字面量 `map[string]int{}`。

```go
count := map[string]int{}
count["open"]++
n, ok := count["done"]
fmt.Println(n, ok) // 0 false
delete(count, "open")
```

逗号 ok 用来区分「没有这个键」和「值恰好是零值」。遍历顺序是故意打乱的，不要依赖它。需要稳定顺序时，把键收进切片再排序。

并发读写同一个 `map` 会 panic。每个请求自己的局部 `map` 没有这个问题。跨请求共享的缓存要用互斥锁，或用 `sync.Map`。`sync.Map` 适合键集合稳定、读多写少的场景，不是普通 `map` 的默认替换。

### 4.3 结构体、嵌入与指针

结构体是一组字段。字段名大写才能被别的包和 `encoding/json` 看到。标签写在字段后面。

```go
type Habitat struct {
    Burrow string
}

type Gopher struct {
    Name string
    Habitat
}

g := Gopher{
    Name:   "Gopher",
    Burrow: "Burrow #42",
}
```

`Habitat` 没有字段名，这是嵌入。`g.Burrow` 和 `g.Habitat.Burrow` 是同一块数据。Go 1.27 起，字面量可以用被嵌入字段的名字当键，所以上面的 `Burrow:` 可以直接写。更早的版本要写成 `Habitat: Habitat{Burrow: "Burrow #42"}`。

嵌入两个类型、它们又有同名字段时，`g.Name` 这种写法会产生歧义，必须写成 `g.A.Name`。外层若自己有 `Name`，外层优先。

指针保存地址。`*Task` 的零值是 `nil`，对 `nil` 取值会 panic。

```go
func Rename(t *Task, title string) {
    t.Title = title
}
```

调用 `Rename(&task, "写完")` 修改的是调用方的那一份。若参数是 `Task` 值，函数里的修改只影响副本。结构体很大、或方法要修改它时，用指针。小结构体作为只读参数，值传递更直接。切片、`map`、接口本身就带有引用语义：传递切片不会复制全部元素，但会复制切片头。

`new(Task)` 返回 `*Task`，指向一份零值。Go 1.26 起 `new` 可以接受表达式，用来给可选字段填初值：

```go
type Person struct {
    Name string
    Age  *int
}

func yearsSince(n int) int { return n }

p := Person{Name: "Ada", Age: new(yearsSince(36))}
```

`Age` 用指针，是为了区分「没有填写」和「0 岁」。JSON 里配合 `omitempty` 时，`nil` 指针通常被省略。数值 0 若用普通 `int`，编码结果也是 0，调用方分不清是没填还是真的是 0。

结构体可以比较，前提是每个字段都可以比较。切片和 `map` 不能比较，含有它们的结构体也不能用 `==`。需要判断两份任务内容是否相同，就逐字段比较，或比较标识。

### 4.4 练习

1. 解释 `b := a[:2]; b = append(b, 9)` 为什么可能改写 `a`。怎样切才能让 `append` 一定分配新数组？
2. 为什么 HTTP 列表接口要把 `nil` 切片换成空切片？
3. `Age *int` 比 `Age int` 多表达了哪一种状态？

---

## 第 5 章　控制流

### 5.1 if

`if` 的条件必须是 `bool`。它可以带一个短语句，作用域只到这个 `if`/`else` 结束。

```go
if err := store.CreateTask(ctx, db, userID, title); err != nil {
    return err
}
```

`err` 在 `else` 里仍然可见，出了这个 if 就不可见。所以「先处理错误并 return，剩下的代码保持不缩进」是常见写法。不要写成多层 `else` 把成功路径挤到最里面。

### 5.2 for

Go 只有 `for`。三种样子：

```go
for { // 一直循环，函数里用 return 或 break 离开
}

for i := 0; i < len(tasks); i++ {
}

for i, task := range tasks {
    _ = i
    _ = task.Title
}
```

`range` 切片时，值是元素的副本。修改 `task.Title` 不会写回切片。要修改元素就用下标：`tasks[i].Done = true`。`range` map 时，删除当前键是允许的，新增的键是否被访问到没有保证。

Go 1.22 起，`for i := range n` 可以遍历整数 `0 .. n-1`。`n` 必须是整数。

同一版本改了循环变量的语义：每次迭代有自己的 `i` 和 `task`。下面的代码在 Go 1.22 及之后会把每个任务的编号都打印出来。更早的版本会反复看到最后一次的值。

```go
for _, task := range tasks {
    go func() {
        fmt.Println(task.ID)
    }()
}
```

本教程以 1.27 为准，按这个语义写。把变量当作参数传进函数仍然更清楚：`go func(id int64) { fmt.Println(id) }(task.ID)`。读者不用回忆语言版本。

`break` 和 `continue` 作用于最内层循环。要离开外层，给外层加标签：

```go
next:
for _, task := range tasks {
    for _, r := range task.Title {
        if r == ' ' {
            continue next
        }
    }
}
```

`goto` 能跳到同函数的标签，不能跳进新的变量声明。业务代码里几乎用不到。教程不把它当成日常工具。

### 5.3 switch

```go
switch r.Method {
case http.MethodGet, http.MethodHead:
    // 读
case http.MethodPost:
    // 写
default:
    w.WriteHeader(http.StatusMethodNotAllowed)
}
```

`case` 默认自带 `break`。要故意落到下一个 case，写 `fallthrough`。`fallthrough` 不判断下一个 case 的条件，所以很少用。

没有操作数的 `switch` 相当于 `switch true`，每个 case 是一个布尔表达式，适合把一长串 `if err` 的分类写整齐。类型 switch 在第 7 章。

### 5.4 迭代器

Go 1.23 起，`range` 可以遍历函数。标准库用 `iter.Seq[V]` 表示「逐个交出 V」：

```go
// runnable
package main

import (
    "fmt"
    "iter"
)

func countDown(n int) iter.Seq[int] {
    return func(yield func(int) bool) {
        for i := n; i > 0; i-- {
            if !yield(i) {
                return
            }
        }
    }
}

func main() {
    for n := range countDown(3) {
        fmt.Println(n)
        if n == 2 {
            break
        }
    }
}
```

`yield` 返回 false 时，迭代器应停止。`break`、`return` 都会让 `yield` 返回 false。不要在 `yield` 返回 false 之后继续调用它，那会 panic。

`slices.Collect` 把迭代器收进切片。`maps.Keys` 返回 `map` 键的迭代器。自己写 Web 业务时，多数循环仍然是 `range` 切片。迭代器适合库函数把「怎么遍历」藏起来，又不强迫调用方先分配一个切片。

### 5.5 练习

1. `range` 得到的结构体是副本。要把它的 `Done` 改成 true，下标写法是什么？
2. 为什么本教程仍建议把循环变量当作参数传给 goroutine？
3. `fallthrough` 会不会重新检查下一个 case 的条件？

---

## 第 6 章　函数

### 6.1 多返回值

```go
func Divide(a, b int) (int, error) {
    if b == 0 {
        return 0, errors.New("divide by zero")
    }
    return a / b, nil
}
```

调用方必须接收全部结果，或用 `_` 明确丢掉。错误作为最后一个返回值，成功时的错误是 `nil`。

命名返回值让文档更清楚，也让 `defer` 能修改结果：

```go
func Load(id int64) (task Task, err error) {
    defer func() {
        if err != nil {
            err = fmt.Errorf("load %d: %w", id, err)
        }
    }()
    if id <= 0 {
        return Task{}, errors.New("bad id")
    }
    task = Task{ID: id, Title: "示例"}
    return task, nil
}
```

`return` 执行时，先把返回值赋给命名结果，再运行 defer，然后函数才真正返回。defer 里给 `err` 赋值，调用方会看到包装后的错误。

只有 `return`、没有表达式的写法叫裸返回。函数一长，读者就看不清返回的是哪几个变量。本教程的业务函数使用带表达式的 `return`。

### 6.2 可变参数与函数值

```go
func Sum(nums ...int) int {
    total := 0
    for _, n := range nums {
        total += n
    }
    return total
}
```

`nums` 的类型是 `[]int`。已有切片要展开时写 `Sum(items...)`。可变参数必须是最后一个。

函数是值。可以放进变量、当作参数、当作返回值。

```go
type Middleware func(http.Handler) http.Handler
```

这就是后面 HTTP 中间件的类型：吃进一个处理器，返回一个包了一层的处理器。

### 6.3 闭包

闭包捕获的是变量，不是变量当时的值。

```go
func Counter() func() int {
    n := 0
    return func() int {
        n++
        return n
    }
}
```

两次调用返回的函数，看到的是同一个 `n`。这在中间件里很有用：外层函数拿着日志器或数据库，内层函数是真正的 `ServeHTTP`。

### 6.4 defer

`defer` 把函数调用推迟到当前函数返回前，后进先出。参数在执行 `defer` 这一行时就求值，不是在函数返回时。

```go
func ReadFirst(path string) (int, error) {
    f, err := os.Open(path)
    if err != nil {
        return 0, err
    }
    defer f.Close()
    var b [1]byte
    n, err := f.Read(b[:])
    return n, err
}
```

打开成功之后立刻 `defer f.Close()`，后面无论从哪一个 `return` 离开，文件都会关上。循环里对大量文件 `defer`，这些关闭要等到函数结束才发生，描述符会堆着。循环内要提前关闭时，把单次处理放进单独的函数，让 defer 在那次调用结束时运行。

`defer` 与锁成对出现：`mu.Lock(); defer mu.Unlock()`。

### 6.5 练习

1. 命名返回值配合 `defer` 时，包装错误的赋值为什么能被调用方看到？
2. `defer f.Close()` 的 `f` 在什么时候被确定？如果 `defer` 之前把 `f` 重新赋给另一个文件，关上的是哪一个？
3. 为什么不在一个处理 10 万个文件的循环里直接 `defer` 每个文件的 `Close`？

---

## 第 7 章　方法与接口

### 7.1 方法与接收者

方法是带接收者的函数。接收者写在 `func` 和名字之间。

```go
type Task struct {
    Title string
    Done  bool
}

func (t Task) Label() string {
    if t.Done {
        return "[x] " + t.Title
    }
    return "[ ] " + t.Title
}

func (t *Task) Finish() {
    t.Done = true
}
```

值接收者得到副本，`Label` 不修改任务。指针接收者可以修改原值。选择一条就坚持：一个类型上混用两种接收者是允许的，但读者要逐个方法回忆会不会改数据。有任何一个方法需要指针时，通常全部用指针。

方法集决定谁能实现接口：

- 值 `T` 只有值接收者方法。
- 指针 `*T` 既有值接收者方法，也有指针接收者方法。

因此 `Finish` 是指针接收者时，`*Task` 实现了要求 `Finish()` 的接口，`Task` 值没有实现。把 `Task` 放进那个接口会编译失败。可寻址的值会自动取地址：`task.Finish()` 在 `task` 是变量时合法。`map` 里取出来的结构体元素不可寻址，不能直接调用指针接收者方法。

`nil` 指针可以调用值接收者不碰字段的方法，也可以调用指针接收者，只要方法内部先判断 `t == nil`。不判断就读取 `t.Title` 会 panic。

### 7.2 接口是隐式的

接口列出方法。一个具体类型拥有这些方法，它就实现了接口，不用写 `implements`。

```go
type Writer interface {
    Write(p []byte) (int, error)
}
```

`os.File`、`bytes.Buffer`、`http.ResponseWriter` 的具体类型都实现了 `io.Writer` 的这一部分。接口越小越好。标准库的 `io.Reader` 只有 `Read`。HTTP 的扩展点 `http.Handler` 只有 `ServeHTTP`。任务板没有发明大接口。存储函数就是普通函数，测试时换一个 `*sql.DB` 指向临时文件。

接口值里有两样东西：具体类型，和那个类型的值。两者都是空的，接口才等于 `nil`。

```go
// runnable
package main

import (
    "bytes"
    "fmt"
    "io"
)

func main() {
    var buf *bytes.Buffer
    var r io.Reader = buf
    fmt.Println(buf == nil) // true
    fmt.Println(r == nil)   // false
}
```

`r` 的具体类型是 `*bytes.Buffer`，值是 `nil`。接口本身不是 `nil`。函数若返回 `(io.Reader, error)`，失败时要 `return nil, err`，不要返回一个持有 `nil` 指针的接口。调用方的 `if r != nil` 会走错分支，接着调用方法时 panic。

空接口 `interface{}` 的别名是 `any`。它什么方法都不要求，所以任何值都能放进去。`any` 不是「任意操作都合法」：要从里面取出 `string`，必须做类型断言。能用具体类型时就用具体类型。JSON 解码到未知结构时才会见到 `map[string]any`。

### 7.3 类型断言与类型 switch

```go
if pathErr, ok := err.(*os.PathError); ok {
    fmt.Println(pathErr.Path)
}
```

`ok` 为 false 时，断言失败不会 panic。去掉 `ok` 的写法 `err.(*os.PathError)` 在失败时 panic。业务分支用带 `ok` 的形式。

```go
switch v := rec.(type) {
case *os.PathError:
    fmt.Println("path", v.Path)
case nil:
    fmt.Println("nil")
default:
    fmt.Printf("other %T\n", v)
}
```

类型 switch 里没有 `fallthrough`。

### 7.4 嵌入接口

```go
type ReadWriter interface {
    io.Reader
    io.Writer
}
```

嵌入是把方法集合并进来。`http.ResponseWriter` 就嵌入了 `io.Writer`。这和结构体嵌入是同一语法、不同效果：接口嵌入合并方法，结构体嵌入提升字段和方法。

### 7.5 练习

1. 为什么把 `nil` 的 `*bytes.Buffer` 放进 `io.Reader` 之后，接口值不等于 `nil`？
2. 方法是指针接收者时，`Task` 值和 `*Task` 哪一个实现了对应接口？
3. 一个接口里放 15 个方法，和拆成 3 个各 5 个方法的小接口，测试时哪一种更容易做假实现？

---

## 第 8 章　错误

### 8.1 error 是接口

```go
type error interface {
    Error() string
}
```

`errors.New` 和 `fmt.Errorf` 返回实现了这个接口的值。比较错误不要比较字符串。字符串会为了读起来更清楚而被改写，调用方的判断就会碎掉。

用哨兵值表示可预期的分类：

```go
var (
    ErrInvalid  = errors.New("invalid")
    ErrConflict = errors.New("conflict")
    ErrNotFound = errors.New("not found")
)
```

任务板的注册、登录和任务查询都返回这三个之一，或用 `%w` 包起来的更底层错误。

### 8.2 包装、Is 与 As

```go
if err != nil {
    return fmt.Errorf("insert user: %w", err)
}
```

`%w` 保留原错误。`errors.Is(err, sql.ErrNoRows)` 会沿着包装链比较。`errors.As` 把链上第一个匹配的具体类型取出来：

```go
var pathErr *os.PathError
if errors.As(err, &pathErr) {
    fmt.Println(pathErr.Path)
}
```

`%v` 只是把文字嵌进去，`errors.Is` 找不到原来的哨兵。要让上层判断，就用 `%w`。日志里需要路径和编号时，把这些上下文写进包装文字：`fmt.Errorf("load task %d: %w", id, err)`。

`errors.Join(err1, err2)` 把多个错误收成一个，`errors.Is` 对其中每一个都有效。适合一次关闭多个资源时把错误汇总。

自定义类型也可以是错误：

```go
type FieldError struct {
    Field string
    Msg   string
}

func (e *FieldError) Error() string {
    return e.Field + ": " + e.Msg
}
```

返回时用指针还是值，要和 `errors.As` 的目标一致。返回 `&FieldError{...}`，`As` 的目标就是 `**FieldError` 对应的 `*FieldError` 变量地址。

### 8.3 什么时候用 panic

`panic` 展开当前 goroutine 的调用栈，运行沿途的 defer，直到被 `recover` 接住，或进程崩溃。

适合 panic 的情况：程序自己写出了不可能的状态，例如正则是编译期常量却编译失败、`init` 里 bcrypt 这种不会失败的步骤失败了。任务板的 `init` 在生成占位哈希失败时 panic，因为那说明二进制或运行时坏了，继续跑只会让登录判断失去意义。

不适合 panic 的情况：用户标题为空、邮箱重复、数据库忙、请求超时。这些都是 `error`。HTTP 服务器会接住处理函数里的 panic，记下栈，关掉这条连接。那是最后一道护栏，不是业务分支的写法。

```go
defer func() {
    if v := recover(); v != nil {
        slog.Error("recovered", "panic", v)
    }
}()
```

自己写库时不要随手 `recover`。把 panic 吞掉会隐藏缺陷。服务器和最外层的 `main` 才是考虑接住它的地方。`net/http` 已经接住了处理函数。

### 8.4 练习

1. `fmt.Errorf("insert: %v", err)` 和 `%w` 对 `errors.Is` 有什么差别？
2. 邮箱重复应该返回哨兵错误，还是 panic？
3. 为什么失败的函数要 `return nil, err`，而不是返回一个里面装着 `nil` 指针的接口？

---

## 第 9 章　泛型

### 9.1 类型参数

类型参数写在函数名或类型名后面的方括号里。约束写在参数后面。

```go
func First[E any](items []E) (E, bool) {
    if len(items) == 0 {
        var zero E
        return zero, false
    }
    return items[0], true
}
```

`any` 表示不要求任何操作。调用时常常能推断：`First([]string{"a", "b"})` 不必写成 `First[string](...)`。推断不了时再写明。

约束是一个接口。`comparable` 表示可以用 `==` 和 `!=`，键和集合需要它：

```go
func Contains[E comparable](items []E, target E) bool {
    for _, item := range items {
        if item == target {
            return true
        }
    }
    return false
}
```

联合约束列出允许的类型。`~` 表示底层类型是它的具名类型也算：

```go
type Number interface {
    ~int | ~int64 | ~float64
}

func Sum[T Number](nums []T) T {
    var total T
    for _, n := range nums {
        total += n
    }
    return total
}
```

`type ID int64` 的底层类型是 `int64`，`~int64` 接受它，单独写 `int64` 不接受。

类型参数也可以放在类型上：

```go
type Pair[A, B any] struct {
    Left  A
    Right B
}
```

Go 1.26 起，约束可以引用正在定义的类型自身：

```go
type Adder[A Adder[A]] interface {
    Add(A) A
}
```

这让「这个类型能和自己相加」可以写进约束。日常的任务板用不到。知道限制已经放开即可，读到标准库或第三方库时不会以为这是非法写法。

### 9.2 泛型方法

Go 1.27 起，具体类型的方法可以有自己的类型参数。

```go
// runnable
package main

import "fmt"

type List[E any] struct {
    items []E
}

func (l List[E]) Map[R any](f func(E) R) List[R] {
    out := make([]R, len(l.items))
    for i, v := range l.items {
        out[i] = f(v)
    }
    return List[R]{items: out}
}

func main() {
    got := List[int]{items: []int{0, 2, 4}}.
        Map(func(v int) int { return v + 2 }).
        Map(func(v int) int { return v / 2 })
    fmt.Println(got.items)
}
```

`Map` 的类型参数 `R` 和 `List` 自己的 `E` 不是一回事。链式调用从左到右读。在 1.27 之前，同样的功能只能写成包级函数 `MapList(list, f)`，调用时要从里往外套。

两条限制要记住：

- 接口方法不能声明类型参数。
- 接口方法也不能由泛型方法来实现。泛型方法属于具体类型，用来组织那个类型自己的操作。

`math/rand/v2` 的 `(*Rand).N[Int intType](Int) Int` 是标准库里的例子：一个方法覆盖全部整数类型，不用再为 `int32`、`int64` 各写一个。

函数类型推断在 1.27 里放宽了。泛型函数赋给一个具体函数类型的变量、放进复合字面量、做类型转换或送进通道时，可以推断类型参数。

```go
func GenericFormatter[T any](v T) string {
    return fmt.Sprintf("value: %v", v)
}

type IntFormatter func(int) string

var formatters = []IntFormatter{GenericFormatter}
```

### 9.3 什么时候不用泛型

只为一种类型服务的函数，写成普通函数。`CreateTask` 不需要类型参数。泛型适合切片、`map`、集合、以及「同一种形状、元素类型不同」的容器。为了少写一次 `int` 版本和 `string` 版本，就值得用。为了让一个 Web 处理函数看起来更抽象，就不值得。

标准库已经提供了大部分常用操作：`slices`、`maps`、`cmp`。自己先找这些包，再决定要不要写新的泛型函数。

### 9.4 练习

1. `Contains` 的约束为什么是 `comparable` 而不是 `any`？
2. 接口里能不能声明 `Map[R any](func(E) R) List[R]` 这样的方法？
3. 任务板的 `CreateTask` 要不要改成泛型？说明理由。

---

## 第 10 章　并发

这一章是语言部分里最需要慢慢读的一章。HTTP 服务器已经替你为每条连接启动了 goroutine。你自己还是会为超时、后台清理和测试写并发。

### 10.1 goroutine

`go f(x)` 启动一个 goroutine。参数 `x` 立刻求值，然后函数在新的 goroutine 里跑。`main` 返回时，进程退出，其他 goroutine 不再继续。服务器的 `main` 因此要一直阻塞到收到退出信号，第 20 章会写这个等待。

不要为了「看起来异步」就把每个函数调用都放进 goroutine。一次数据库查询在当前请求的 goroutine 里做即可。请求取消时，把 `r.Context()` 传进查询，驱动会在取消后停止等待。

### 10.2 通道

通道是带类型的传送带。`ch := make(chan Task)` 是无缓冲通道：发送方和接收方都到了，传递才发生。`make(chan Task, 4)` 是容量为 4 的缓冲：缓冲没满时发送不等待接收方。

```go
ch <- task   // 发送
task := <-ch // 接收
```

方向可以写进类型。`chan<- Task` 只能发送，`<-chan Task` 只能接收。函数参数写成单向通道，调用方就知道这个函数不会把通道用反。

关闭：

- 只有发送方关闭。
- 关闭表示「不会再有值」。
- 对已关闭通道接收，立刻得到零值和 `ok == false`。
- `for v := range ch` 在通道关闭且取空之后结束。
- 向已关闭通道发送，或重复关闭，会 panic。
- 接收方不要关闭，多个发送方不要各自关闭。不确定谁来关闭时，就不要关闭，改用 `context` 通知结束。

`nil` 通道上的发送和接收永远阻塞。在 `select` 里，`nil` 通道的 case 相当于不存在。可以把 case 的通道设成 `nil` 来动态关掉一条分支。

### 10.3 select

`select` 等待多个通信里的任意一个。有多个就绪时，随机选一个。`default` 在没有通信就绪时立刻执行。

```go
// runnable
package main

import (
    "context"
    "fmt"
    "time"
)

func main() {
    ctx, cancel := context.WithTimeout(context.Background(), 10*time.Millisecond)
    defer cancel()

    ch := make(chan string, 1)
    go func() {
        time.Sleep(50 * time.Millisecond)
        ch <- "done"
    }()

    select {
    case msg := <-ch:
        fmt.Println(msg)
    case <-ctx.Done():
        fmt.Println(ctx.Err())
    }
}
```

这个程序会打印 `context deadline exceeded`。工作还在睡眠，但 `select` 已经因为超时离开。那个 goroutine 随后仍会向 `ch` 发送。因为通道有缓冲，发送能完成，goroutine 能退出。如果通道无缓冲，而已经没有人接收，这个 goroutine 会永远堵在发送上。这就是泄漏：函数看起来返回了，栈和它捕获的变量还留着。

避免泄漏的办法是让发送也能被取消，或保证总有缓冲、总有接收方。测试可以用 Go 1.27 的 `goroutineleak` profile 观察仍然堵着的 goroutine。`net/http/pprof` 把它暴露在 `/debug/pprof/goroutineleak`。

### 10.4 context

`context.Context` 携带取消、截止时间和请求范围内的值。函数的第一个参数习惯是 `ctx context.Context`。不要把 `Context` 放进结构体再到处传递，请求处理函数从 `r.Context()` 拿，显式传给存储层。

```go
ctx, cancel := context.WithTimeout(r.Context(), 2*time.Second)
defer cancel()
task, err := store.CreateTask(ctx, db, userID, title)
```

`defer cancel()` 要写。即使超时已经发生，`cancel` 也会释放定时器。`WithCancel` 适合「用户点了停止」。`Background` 用在 `main` 和测试的根上。`TODO` 表示你还不知道这个调用属于哪一次请求，它应该在代码评审里被看成待办，而不是永久写法。

`WithValue` 能在上下文里放值。键要用未导出的私有类型，避免和别的包相撞。任务板只放当前用户和会话令牌，这是请求范围内、中间件已经查过的数据。不要把数据库、日志器或可选参数塞进 `Context`。那些应该是结构体字段或普通参数。

取消是协作的。`ctx.Err()` 变成非 `nil` 不会魔术般停掉你的纯计算循环。循环要自己看 `ctx.Done()`。`database/sql` 的 `QueryContext`、`ExecContext` 会看上下文。所以存储层的函数都把 `ctx` 传进 SQL。

### 10.5 互斥锁与等待

共享内存时用锁。`sync.Mutex` 的零值可用。不要复制锁，复制之后就变成两把锁。含有锁的结构体通过指针传递。

```go
type Counter struct {
    mu sync.Mutex
    n  int
}

func (c *Counter) Add(d int) {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.n += d
}
```

`RWMutex` 允许多个读者或一个写者。读多写少时它能减少等待。任务板的共享状态在 SQLite 里，数据库自己负责锁。内存里的计数器才用 `Mutex`。

`sync.WaitGroup` 等待一组 goroutine 结束：

```go
var wg sync.WaitGroup
for _, id := range ids {
    wg.Add(1)
    go func() {
        defer wg.Done()
        process(id)
    }()
}
wg.Wait()
```

`Add` 要在 `go` 之前调用。放在 goroutine 里面，`Wait` 可能抢先看到 0。Go 1.25 起 `WaitGroup` 还有 `Go` 方法，把 `Add`、启动和 `Done` 收成一次调用。本教程仍写出 `Add` 和 `Done`，因为你在旧代码里会一直见到它们。

`sync.Once` 保证某段初始化只跑一次，并发调用也会等待那一次完成。适合懒加载配置。失败的初始化也会被当成「已经做过」。会失败的初始化要自己记录错误，下一次才能重试。`Once` 不会自动重试。

`sync.Pool` 复用临时对象，降低分配。对象可能随时被清掉，不能假设取出来的内容还在。任务板不需要它。知道它存在，是为了以后读标准库时不吃惊。

### 10.6 两种常用形状

工作者池：固定数量的 goroutine 从通道取任务。用在「可以并行、但要限制并发」的场合，例如同时回源的数量。

```go
func RunPool(ctx context.Context, jobs <-chan int, workers int, fn func(int)) {
    var wg sync.WaitGroup
    for range workers {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for {
                select {
                case <-ctx.Done():
                    return
                case job, ok := <-jobs:
                    if !ok {
                        return
                    }
                    fn(job)
                }
            }
        }()
    }
    wg.Wait()
}
```

这里有一个选择问题：`select` 在任务和取消同时就绪时随机挑选。工作者可能在取消之后又做完一个任务。对任务板的 HTTP 请求，这个精度足够。若取消必须立刻压过任务，就在 `fn` 里面再看 `ctx.Err()`。

流水线：一个阶段的输出通道是下一个阶段的输入。每个阶段在自己的 goroutine 里跑，用关闭通道表示「这一段结束了」。扇出是多个工作者读同一个通道，扇入是多个结果通道写进一个。扇入时用 `WaitGroup` 等全部发送方结束，再关闭输出通道。由接收方关闭会和发送方冲突。

`golang.org/x/sync/errgroup` 把「一组 goroutine、其中一个错误就取消其余的」封装好了。它不是标准库。任务板的请求路径用不到，做批量任务时可以再引入。

### 10.7 数据竞争

两个 goroutine 同时访问同一块内存，至少一方在写，又没有同步，这就是数据竞争。结果是不确定的。`go test -race` 或 `go run -race` 会在运行时报告。竞态检测让程序变慢、变费内存，提交前对并发代码开一次，而不是每个本地命令都开。

循环变量的捕获在 1.22 之后已经按迭代分开。共享切片元素、共享 `map`、共享结构体字段，仍然会竞争。修法是不要共享，或用通道移交所有权，或加锁。

### 10.8 练习

1. 无缓冲通道上，只有发送方、没有接收方时，发送方会怎样？
2. 谁负责关闭通道？接收方关闭会发生什么？
3. 为什么 `WaitGroup.Add` 要写在 `go` 语句之前？

---

## 第 11 章　模块

### 11.1 go.mod

`go mod init example.com/taskboard` 生成 `go.mod`。第一行是模块路径，第二行是工具链按哪个语言版本解释源码。任务板写 `go 1.27`，因为代码使用这个版本已经稳定的标准库。

依赖写在 `require` 里。直接用到的包是直接依赖，它们再带进来的是间接依赖，`go mod tidy` 会标上 `// indirect`。

```text
module example.com/taskboard

go 1.27

require (
    github.com/go-chi/chi/v5 v5.2.3
    golang.org/x/crypto v0.41.0
    modernc.org/sqlite v1.38.2
)
```

版本号遵循语义化版本：`v主版本.次版本.补丁`。主版本表示不兼容的 API 变化。Go 把主版本写进导入路径，从 v2 开始路径以 `/v2` 结尾。chi 的 v5 因此是 `github.com/go-chi/chi/v5`。v0 和 v1 不写在路径里。升级补丁和次版本通常只改 `go.mod` 里的版本再 `go mod tidy`。升级主版本等于换一条导入路径，调用代码要跟着改。

`go.sum` 记录每个模块版本的校验和。提交它。别人构建时，下载到的代码若和校验和不符，构建会停。

### 11.2 最小版本选择

Go 的构建会选出每个模块「被依赖图要求的最高版本」，而不是最新的版本。A 要 chi v5.0.0，B 要 chi v5.2.3，最终用 v5.2.3。这要求 v5.2.3 仍然能编译按 v5.0.0 写的代码。语义化版本是这个规则的前提。不要在次版本里删掉已经导出的函数。

`exclude` 可以排除一个已知有问题的版本。`replace` 把某个模块临时指到另一条路径或另一个版本：

```text
replace example.com/store => ../store
```

适合本地同时改两个模块。提交给别人之前删掉只在你机器上成立的 `replace`。

### 11.3 工作区

两个模块要一起改时，用 `go.work`：

```bash
go work init ./taskboard ./tools
```

工作区文件通常不提交。它只影响你这台机器上的构建，让导入路径仍然写成模块路径，实际代码从旁边的目录读。

### 11.4 日常命令

```bash
go get github.com/go-chi/chi/v5@v5.2.3
go mod tidy
go list -m all
```

`tidy` 会删掉源码里已经不用的依赖，补上缺少的。只加驱动的空白导入 `_ "modernc.org/sqlite"` 也算使用，`tidy` 会保留它。

私有模块如果校验和服务访问不到，设置 `GOPRIVATE=example.com`。任务板的依赖都是公开的，不需要这一步。

### 11.5 练习

1. 为什么 chi v5 的导入路径里有 `/v5`，而 `golang.org/x/crypto` 没有？
2. `go.sum` 为什么要提交？
3. `replace` 指向你自己的磁盘路径时，同事克隆仓库后会发生什么？

---

## 第 12 章　测试

### 12.1 表驱动测试

测试文件以 `_test.go` 结尾，测试函数以 `Test` 开头，参数是 `*testing.T`。

```go
func TestNormalizeTitle(t *testing.T) {
    tests := []struct {
        name string
        in   string
        want string
        ok   bool
    }{
        {name: "trim", in: "  买牛奶  ", want: "买牛奶", ok: true},
        {name: "blank", in: "   ", ok: false},
        {name: "too long", in: strings.Repeat("题", 201), ok: false},
    }
    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got, err := NormalizeTitle(tt.in)
            if tt.ok && (err != nil || got != tt.want) {
                t.Fatalf("got %q %v", got, err)
            }
            if !tt.ok && !errors.Is(err, ErrInvalid) {
                t.Fatalf("err %v", err)
            }
        })
    }
}
```

`t.Run` 让每行有自己的名字，失败时能看到是哪一行。`t.Fatal` 立刻结束当前测试函数。`t.Error` 记录失败并继续。辅助函数第一行写 `t.Helper()`，失败行号会指向调用它的测试，而不是辅助函数内部。

`t.Parallel()` 让测试并行。并行测试不要共享可写的全局变量，也不要一起写同一个数据库文件。任务板的测试各自用 `t.TempDir()` 里的数据库文件，结束时由测试框架删掉。

### 12.2 示例、基准、覆盖率、模糊测试

`Example` 函数的输出写在注释里，`go test` 会核对：

```go
func ExampleNormalizeTitle() {
    title, _ := NormalizeTitle("  买牛奶 ")
    fmt.Println(title)
    // Output:
    // 买牛奶
}
```

示例会出现在 `go doc` 里。基准测试看的是分配和耗时，不是对错：

```go
func BenchmarkNormalizeTitle(b *testing.B) {
    for b.Loop() {
        _, _ = NormalizeTitle("买牛奶")
    }
}
```

`go test -bench=. -benchmem` 运行基准。`b.Loop` 是较新的写法，框架负责循环次数。旧代码里常见 `for i := 0; i < b.N; i++`，两种都会见到。

`go test -cover ./...` 打印覆盖率。覆盖率低说明有的分支没跑到，覆盖率高不说明边界都对。任务板至少盖住：注册成功、邮箱重复、用户之间不能改对方的任务、表单创建、HTMX 片段、错误的 Origin、JSON 注册和越权删除。

模糊测试把随机字节送进解析函数：

```go
func FuzzNormalizeTitle(f *testing.F) {
    f.Add("买牛奶")
    f.Fuzz(func(t *testing.T, s string) {
        got, err := NormalizeTitle(s)
        if err == nil && (got == "" || len([]rune(got)) > 200) {
            t.Fatalf("accepted %q", got)
        }
    })
}
```

`go test -fuzz=FuzzNormalizeTitle` 会跑一段时间。解析外部输入的函数适合做模糊测试。标题校验就是这种函数。

### 12.3 HTTP 与时间

`net/http/httptest` 提供 `NewRecorder` 和 `NewRequest`，处理函数不用真的监听端口。`httptest.NewServer` 会听一个临时端口，适合从真实 HTTP 客户端打进来。任务板的测试用 `ResponseRecorder`，因为要看的是状态码、Cookie 和正文。

`testing/synctest` 在一个隔离的「气泡」里跑并发代码。气泡里的 `time.Now` 从 2000-01-01 起算，所有 goroutine 都堵住时时间才前进。用来测试超时和 `context`，不用真的睡几秒。网络阻塞不算「可被气泡推进的等待」，所以气泡里不要连真实套接字。第 25 章的测试用临时数据库文件，不依赖 `synctest`。

```go
func TestTimeout(t *testing.T) {
    synctest.Test(t, func(t *testing.T) {
        ctx, cancel := context.WithTimeout(t.Context(), 5*time.Second)
        defer cancel()
        time.Sleep(5 * time.Second)
        synctest.Wait()
        if ctx.Err() != context.DeadlineExceeded {
            t.Fatalf("ctx.Err() = %v", ctx.Err())
        }
    })
}
```

`t.Context()` 在测试结束或失败时取消。查询和请求用它，测试结束后不应再有后台查询。

### 12.4 练习

1. `t.Fatal` 和 `t.Error` 的差别是什么？
2. 两个并行测试为什么不能共用同一个 SQLite 文件？
3. 覆盖率 100% 之后，还需要看哪些测试没写？举一个任务板的例子。

---

## 第 13 章　写 Web 时会反复用到的标准库

这一章按任务板会碰到的包来讲。每个包只讲会用到的那一部分。

### 13.1 fmt、strings、strconv

`fmt.Sprintf` 和 `fmt.Errorf` 的动词要和参数匹配。`%s` 字符串，`%d` 整数，`%v` 默认格式，`%q` 带引号，`%w` 只在 `Errorf` 里表示包装错误。`go vet` 能发现动词和参数对不上。

`strings.TrimSpace`、`ToLower`、`Contains`、`HasPrefix`、`Cut` 覆盖大部分文本处理。邮箱规范化是 `ToLower(TrimSpace(s))`。这不是完整的邮件地址标准，教程里会标明。

`strconv.ParseInt(s, 10, 64)` 把十进制文字转成 `int64`。路径里的任务编号用它。失败就 404 或 400，不要把解析错误原样写给用户。`strconv.Itoa` 做相反的事。

### 13.2 time

统一用 UTC 存进数据库：`time.Now().UTC()`，格式 `time.RFC3339Nano`。读出来用 `time.Parse(time.RFC3339Nano, text)`。页面上要显示香港时间时再 `In(time.FixedZone("HKT", 8*60*60))`，或加载 `Asia/Hong_Kong`。不要把本地时区的字符串存进库，换一台机器就对不齐。

`time.Duration` 是纳秒数。`5 * time.Second` 这种写法才可读。不要自己乘 1000000000。

超时用 `context.WithTimeout`，不要在每个 SQL 调用外面再套一层 `time.After` 却继续让查询跑着。

### 13.3 io、os、embed

`io.Reader` 和 `io.Writer` 是流的接口。HTTP 正文是 `io.ReadCloser`，用完要关。`http.Server` 会替你关请求的 `Body`。你自己发出的客户端请求，必须 `defer resp.Body.Close()`，并在关闭前读完或丢弃剩余字节，连接才能复用。`io.Copy(io.Discard, resp.Body)` 可以丢弃。

`os.Getenv` 读配置。任务板读 `TASKBOARD_ADDR` 和 `TASKBOARD_DSN`，没有就用默认值。缺了必填项才退出。地址有默认值 `:8080`，所以本地直接运行即可。

`embed` 在编译期把文件打进二进制：

```go
//go:embed templates/*.html
var templateFS embed.FS
```

指令前面不能有空行，和 `var` 之间也不能有别的语句。路径相对于源码文件所在目录。任务板的模板因此不依赖运行时的当前目录。容器里只有一个可执行文件时，这是必要的。

### 13.4 log/slog

`log/slog` 是标准库的结构化日志。

```go
log := slog.New(slog.NewTextHandler(os.Stdout, nil))
log.Info("listen", "addr", addr)
log.Error("open database", "err", err)
```

键和值成对出现。不要把用户密码放进日志。请求日志记方法、路径、状态码和耗时。正文里的标题可以记，密码和会话令牌不要记。

生产环境可以把 `NewJSONHandler` 接到标准输出，由外部系统收集。教程本地用文本，读起来直接。

### 13.5 encoding/json 与 v2

Go 1.27 有两套都可用的 API。

`encoding/json` 是原来的 API，现有代码继续用。1.27 里它的实现换成了 v2，但对外行为按 v1 保持，包括重复键的处理和错误文字的大体含义。`Marshal`、`Unmarshal`、`NewEncoder`、`NewDecoder`、`DisallowUnknownFields` 都还在。

`encoding/json/v2` 是新 API。`Marshal` 和 `Unmarshal` 接受可选的 `Options`。默认更严：拒绝非法 UTF-8，拒绝对象里的重复名字。`jsontext` 子包按记号流处理 JSON。

```go
import jsonv2 "encoding/json/v2"

b, err := jsonv2.Marshal(task)
err = jsonv2.Unmarshal([]byte(`{"a":1,"a":2}`), new(map[string]int))
// err != nil，因为重复键
```

任务板的 HTTP 层使用 v1 API，原因是 `DisallowUnknownFields`、`Decoder` 和现有教程材料一致，而且 1.27 承诺这条 API 继续可用。第 17 章会指出哪些输入在 v2 默认设置下会被拒绝。新代码若希望默认就拒绝重复键，直接用 v2。

结构体标签：

| 标签 | 含义 |
| --- | --- |
| `json:"title"` | 键名 |
| `json:"title,omitempty"` | 零值时省略 |
| `json:"-"` | 不参与 |
| `json:",omitempty"` | 键名仍用字段名，零值省略 |

未导出字段不会被编码。密码哈希不要出现在任何会序列化的结构体里。任务板的 `User` 没有 JSON 标签，也不会被写进响应。`Task` 有标签，因为它就是 API 的正文。

`omitempty` 对指针特别有用：`nil` 表示省略，指向 0 的指针表示真的有一个 0。

### 13.6 net/url 与 uuid

`url.Parse` 解析 Origin，用来做同站检查。`url.Values` 是表单和查询字符串的键值。`URL.Clone` 与 `Values.Clone` 在 1.27 里做深拷贝，改副本不会动到请求自己的 URL。

标准库新增了 `uuid` 包：`uuid.NewV7()` 生成带时间顺序的 UUID，`uuid.Parse` 解析。任务板的会话令牌用 32 字节加密随机数的十六进制，不是 UUID。两种都可以。随机令牌的代码只用 `crypto/rand`，读起来更短，所以教程选它。需要和时间排序的外部标识时再用 `uuid.NewV7`。

### 13.7 练习

1. 为什么数据库里的时间用 UTC 的 RFC3339？
2. `embed` 指令和变量之间插入一行普通注释以外的代码，会发生什么？
3. 同一个 JSON 对象里出现两个 `title` 键时，v1 API 和 v2 默认行为有什么不同？

---

## 第 14 章　Go 的主要开发方向

这一章只建立地图。后面的正文把 Web 前后端写完。其他方向说明它们占用语言的哪一部分，避免学完任务板却不知道 Go 还被用在哪里。

| 方向 | 日常在写什么 | 最吃这门语言的哪一部分 |
| --- | --- | --- |
| Web 后端与 JSON API | 业务接口、管理后台、BFF | 结构体、错误、`net/http`、`database/sql`、并发里的「一个请求一条执行流」 |
| 配套的浏览器界面 | 模板页面，或独立前端调用 API | 模板与 JSON 都只是存储层外面的两种写法 |
| 微服务之间的 RPC | gRPC、Protobuf、超时与重试 | 接口、`context`、错误包装；IDL 生成代码 |
| 命令行工具 | 迁移、代码生成、运维小工具 | 单文件二进制、`flag` 或 `cobra`、明确的退出码 |
| 云原生基础设施 | 控制器、网络代理、网格旁路 | 长驻进程、goroutine、客户端限速、优雅退出 |
| 数据与消息 | 队列消费者、日志管道、批处理 | 工作者池、背压、至少一次投递时的幂等 |
| 系统与网络编程 | 代理、转发、协议解析 | `io.Reader`、缓冲、超时、避免 goroutine 泄漏 |

Go 在这些方向里反复出现，是因为第 0 章的五条设计刚好对齐：一个二进制、编译期类型、垃圾回收、便宜的并发、用返回值把失败写清楚。

它很少被选来做的事也说清楚。重界面的桌面软件通常用专门的 UI 工具。数值计算和训练模型的主战场在别的语言，Go 负责把模型服务包成 API。手机客户端不用 Go 写界面。浏览器里跑 Go 需要编译到 WebAssembly，能做，但不是这条前后端路线的做法。

gRPC 与本教程的 JSON 接口可以并存：对人的浏览器走 JSON，对内的服务走 gRPC。两者都会把 `context` 从入口传到存储层。任务板不拆服务，所以不引入第二套 RPC。

命令行是 Go 最容易做成的副产品。`go build` 得到一个文件，拷到另一台机器就能跑迁移。等任务板需要离线导入数据时，再在 `cmd/import` 里加一个程序，复用 `internal/store`。

云原生组件，例如 Kubernetes 控制器，本质仍是一个长驻进程：监视事件、对比期望状态、调用 API、用工作队列限制并发。语言能力就是第 10 章的池和 `context`，加上生成的客户端。那是另一门部署课，不是另一门语法课。

消息消费者要处理「消息至少到一次」。处理函数要能安全地重复执行。任务板上对应的思想是：用任务编号和用户编号做更新条件，同一条请求执行两次不会改到别人的数据。

读到这里，地图就够用了。从下一章开始，所有例子都服务于任务板。

### 14.1 练习

1. 任务板的 JSON API 和 gRPC 服务都会把哪个参数从入口传到数据库？
2. 为什么「一个文件拷过去就能跑」对命令行工具特别合适？
3. 队列消费者为什么要求处理函数可以重复执行？

---

## 第 15 章　HTTP

### 15.1 请求里有什么

一次 HTTP/1.1 请求包含方法、路径、协议版本、头部和可选正文。方法表示意图：`GET` 读取，`POST` 提交，`PATCH` 局部修改，`DELETE` 删除。`GET` 不应修改数据。浏览器和中间缓存会按这个约定重试 `GET`。

状态码是给程序看的分类，正文是给人看的说明。

| 状态 | 任务板里的用法 |
| --- | --- |
| 200 | 读成功，或 HTMX 返回一段 HTML |
| 201 | 创建了用户或任务 |
| 204 | 删除成功，没有正文 |
| 303 | 表单成功后跳转到列表，避免刷新时重复提交 |
| 400 | JSON 不合法，或标题不合法 |
| 401 | API 没有带有效令牌 |
| 403 | Cookie 请求的 Origin 不对 |
| 404 | 编号不存在，或不属于当前用户 |
| 409 | 邮箱已注册 |
| 500 | 数据库或其他未预期的失败 |

不属于当前用户的任务也返回 404，不返回 403。403 会告诉对方「这个编号存在」。

头部 `Content-Type` 说明正文是什么。页面是 `text/html; charset=utf-8`。JSON 是 `application/json; charset=utf-8`。表单是 `application/x-www-form-urlencoded`。

### 15.2 只用标准库接一个请求

Go 1.22 起，`ServeMux` 能区分方法和路径参数。

```go
// runnable
package main

import (
    "fmt"
    "log"
    "net/http"
    "strconv"
)

func main() {
    mux := http.NewServeMux()
    mux.HandleFunc("GET /healthz", func(w http.ResponseWriter, r *http.Request) {
        w.WriteHeader(http.StatusOK)
        _, _ = w.Write([]byte("ok"))
    })
    mux.HandleFunc("GET /tasks/{id}", func(w http.ResponseWriter, r *http.Request) {
        id, err := strconv.ParseInt(r.PathValue("id"), 10, 64)
        if err != nil {
            http.NotFound(w, r)
            return
        }
        fmt.Fprintf(w, "task %d\n", id)
    })
    log.Fatal(http.ListenAndServe(":8080", mux))
}
```

`r.PathValue("id")` 取出路径里的 `{id}`。`ListenAndServe` 在失败时返回错误，成功时一直阻塞。这个例子用 `log.Fatal` 是因为它还没有自己的日志器。任务板的 `main` 改用 `slog`，并在收到信号时关闭服务器。

`http.Error` 写出状态码和一段纯文本。正式的 API 改用 JSON。纯文本足够用于健康检查。

`http.Request` 的 `Context` 在客户端断开时取消。任何耗时工作都把 `r.Context()` 传下去。

`ListenAndServe` 没有设置 `ReadHeaderTimeout`。慢速客户端可以一点点发送头部，占着连接。正式服务器要设：

```go
srv := &http.Server{
    Addr:              ":8080",
    Handler:           mux,
    ReadHeaderTimeout: 5 * time.Second,
}
```

### 15.3 处理器与中间件

凡是拥有 `ServeHTTP(http.ResponseWriter, *http.Request)` 的类型都是处理器。函数可以用 `http.HandlerFunc` 转成处理器。

中间件是 `func(http.Handler) http.Handler`。它在调用内层之前或之后做一件事：记日志、识别用户、限制正文大小。多个中间件套起来，写在最外层的最先看到请求，最后看到响应。

任务板的请求日志用 chi 的 `middleware.NewWrapResponseWriter` 记住状态码。标准库的 `ResponseWriter` 不提供「最终状态码是多少」的查询。包装器在 `WriteHeader` 时把数字存下来，内层返回后再打日志。

`Recoverer` 接住 panic。自己的中间件若要接，必须 `defer recover`，并仍然把 500 写出去。任务板直接使用 chi 的 `middleware.Recoverer`。

### 15.4 练习

1. 表单创建成功后为什么用 303 跳回列表，而不是 200 后再渲染同一页？
2. 别人的任务编号为什么返回 404？
3. `ReadHeaderTimeout` 在防什么？

---

## 第 16 章　服务骨架

### 16.1 为什么先标准库再 chi

第 15 章的 `ServeMux` 足够做方法和路径。任务板还需要按组挂中间件：一组路由要登录，另一组不要。chi 的 `r.Group` 和 `r.Route` 把这件事写得很直接，处理器仍然是标准的 `http.Handler`。

Gin 在招聘里常见。它自己有一套上下文类型。学它之前先把 `http.ResponseWriter` 和 `*http.Request` 读熟，换框架时业务代码才搬得动。本教程的练习用 chi v5，导入路径是 `github.com/go-chi/chi/v5`。

### 16.2 路由表

任务板的路由分成三块。

```text
GET  /healthz
GET  /
GET  /login          POST /login
GET  /register       POST /register

需要登录，Cookie 或 Bearer 都可以：
GET  /tasks
POST /tasks
POST /tasks/{id}/toggle
POST /tasks/{id}/delete
POST /logout

/api，其中读写需要登录：
POST /api/register
POST /api/login
GET  /api/tasks
POST /api/tasks
PATCH /api/tasks/{id}
DELETE /api/tasks/{id}
POST /api/logout
```

首页只做一件事：303 到 `/tasks`。还没有登录时，`/tasks` 的中间件再 303 到 `/login`。API 在同样的情况下返回 401 和 JSON，因为浏览器脚本无法把 303 理解成「请显示登录表单」之外的更多含义。脚本要看状态码。

`chi.URLParam(r, "id")` 对应路径里的 `{id}`。标准库的 `PathValue` 在 chi 的路由上不要混用。一个项目选定一种路由，就用它的取值函数。

### 16.3 启动和退出

`main` 打开数据库、迁移、解析模板、监听。任何一步失败都写日志并以状态码 1 退出。监听放在单独的 goroutine 里，主 goroutine 等待 `SIGINT` 或 `SIGTERM`。

```go
stop, stopCancel := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
defer stopCancel()
<-stop.Done()

shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
defer cancel()
_ = srv.Shutdown(shutdownCtx)
```

`Shutdown` 停止接受新连接，并等待已有请求结束，最多等五秒。直接退出进程会把写到一半的响应掐掉。

配置来自环境变量。`TASKBOARD_ADDR` 默认 `:8080`。`TASKBOARD_DSN` 默认一个当前目录下的 SQLite 文件，并打开外键和忙等：

```text
file:taskboard.db?_pragma=foreign_keys(1)&_pragma=busy_timeout(5000)
```

`busy_timeout` 让另一个连接正拿着写锁时，当前操作最多等 5 秒，而不是立刻失败。

### 16.4 日志中间件

每个请求记四样东西：方法、路径、状态码、耗时。路径用 `r.URL.Path`，不要把原始查询字符串里的令牌记进去。任务板的令牌在头部或 Cookie 里，不在路径里。仍要养成不记录 `Authorization` 和 `Cookie` 的习惯。

### 16.5 练习

1. 为什么 `/api/tasks` 未登录时返回 401，而 `/tasks` 未登录时返回 303？
2. `Shutdown` 和进程直接退出，对已经进来的请求有什么差别？
3. 记请求日志时，哪两个头部默认不要落盘？

---

## 第 17 章　JSON API

### 17.1 资源长什么样

任务的 JSON：

```json
{
  "id": 1,
  "title": "买牛奶",
  "done": false,
  "createdAt": "2026-10-25T05:30:00.123456789Z"
}
```

创建请求只带 `{"title":"买牛奶"}`。局部修改使用 `PATCH`，字段是指针，这样「没有这个字段」和「字段是零值」才能分开：

```go
type taskInput struct {
    Title *string `json:"title"`
    Done  *bool   `json:"done"`
}
```

`{"done":false}` 会把完成状态改回未完成。`{}` 没有任何修改，返回 400。`{"title":"写完"}` 只改标题。

登录和注册：

```json
{"email":"ada@example.com","password":"correct-horse"}
```

成功时：

```json
{"token":"十六进制的随机会话编号"}
```

失败时正文统一成 `{"error":"..."}`。英文短句给 API，页面上的中文短句给模板。两边不要共用一句写给人看的长文案，否则客户端会开始解析句子。客户端看状态码，句子只帮助调试。

### 17.2 解码

```go
func decodeJSON(w http.ResponseWriter, r *http.Request, dst any) error {
    r.Body = http.MaxBytesReader(w, r.Body, 1<<20)
    dec := json.NewDecoder(r.Body)
    dec.DisallowUnknownFields()
    if err := dec.Decode(dst); err != nil {
        return err
    }
    if err := dec.Decode(&struct{}{}); err != io.EOF {
        return errors.New("trailing json")
    }
    return nil
}
```

`MaxBytesReader` 把正文限制在 1 MiB。超限时解码失败，处理函数返回 400。没有这个限制，客户端可以一直发送正文。

`DisallowUnknownFields` 拒绝结构体上不存在的键。拼错的 `titel` 会得到 400，而不是被悄悄丢掉。

第二次 `Decode` 期望 `io.EOF`。若正文是两个紧挨着的 JSON 值，第一次只读走一个，第二次会成功，函数就返回错误。否则尾部的第二段数据会被忽略。

v2 的 `Unmarshal` 默认拒绝重复键。v1 API 为了兼容，重复键仍按旧规则处理。任务板若改用 `encoding/json/v2`，`{"title":"a","title":"b"}` 会直接失败。现在的解码器依赖 `DisallowUnknownFields` 和「只有一个 JSON 值」，重复键的行为保持 v1。写给外部的 API 若要求更严，把这一处换成 v2，并在发布说明里写明。

编码时设置 `Content-Type`，再 `WriteHeader`，最后 `Encode`。`Encode` 会在末尾加换行。先写正文再改头部是无效的，头部已经随第一次 `Write` 发出。

列表为空时编码 `[]`，不编码 `null`。`ListTasks` 在没有行时返回 `nil` 切片，处理函数在写出之前换成空切片。

### 17.3 状态码与存储错误

存储层返回哨兵错误，HTTP 层决定状态码。

| 哨兵 | HTTP |
| --- | --- |
| `ErrInvalid` | 400 |
| `ErrConflict` | 409 |
| `ErrNotFound` | 404 |
| 其他非 nil 错误 | 500 |

500 的正文不要包含 SQL 或文件路径。那些进日志。客户端只看到 `"request failed"` 或更具体、但不泄漏内部结构的句子。

`DELETE` 成功返回 204，没有 JSON。客户端要把 204 和失败分开，不要对 204 再调用 `response.json()`。第 24 章的 `request` 函数见到 204 就返回 `null`。

### 17.4 练习

1. 为什么 `Done` 用 `*bool`，而不是 `bool`？
2. `MaxBytesReader` 放在解码之前，防的是哪一类客户端？
3. 500 的 JSON 里为什么不返回 `err.Error()` 的原文？

---

## 第 18 章　数据

### 18.1 database/sql

`database/sql` 是接口，驱动是具体实现。任务板使用纯 Go 的 SQLite 驱动 `modernc.org/sqlite`，导入路径空白导入一次，驱动名是 `"sqlite"`。这样 `CGO_ENABLED=0` 也能编译。`github.com/mattn/go-sqlite3` 需要 CGO，和「镜像里只有一个静态二进制」的目标不一致，所以不选它。

```go
import (
    "database/sql"
    _ "modernc.org/sqlite"
)

db, err := sql.Open("sqlite", dsn)
```

`Open` 不一定立刻连上。`PingContext` 才检查连接。连接池由 `*sql.DB` 管理，这个对象应该在进程里长期持有，并在退出时 `Close`。不要每个请求 `Open` 一次。

`SetMaxOpenConns` 限制同时打开的连接数。SQLite 对并发写比较保守，教程把上限设成 8，并设置 `busy_timeout`。写多的服务换 PostgreSQL 时，这个数字按数据库能承受的连接数重新定。

查询一律用 `?` 占位符，参数分开传。不要用 `fmt.Sprintf` 把用户输入拼进 SQL。占位符由驱动转义，标题里的引号仍然只是标题。

```go
row := db.QueryRowContext(ctx,
    `SELECT id, title, done, created_at FROM tasks WHERE id = ? AND user_id = ?`,
    taskID, userID)
```

`user_id` 出现在同一条语句里，是授权的一部分。先按编号查出任务、再在 Go 里比较用户，中间会多一次容易忘的步骤。条件放进 SQL，不属于这个用户时 `RowsAffected` 为 0，函数返回 `ErrNotFound`。

`QueryContext` 返回多行。循环 `rows.Next()`，结束时检查 `rows.Err()`。`defer rows.Close()`。`ExecContext` 用于插入、更新、删除。`LastInsertId` 取 SQLite 刚生成的整数主键。

上下文要传进每一种 `*Context` 方法。用户断开后，驱动可以停止等待锁。

### 18.2 表

```sql
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY,
    email TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    created_at TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS sessions (
    token TEXT PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id),
    expires_at TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS tasks (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id),
    title TEXT NOT NULL,
    done INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL
);
```

SQLite 没有单独的布尔类型，`done` 用 0 和 1。读进 Go 时 `done != 0`。时间用文本保存 UTC 的 RFC3339Nano，排序和解析都稳定。

`CREATE TABLE IF NOT EXISTS` 让 `Migrate` 可以重复执行。这是教学用的迁移。表一旦发布出去，下一版要加列时应另写 `ALTER TABLE`，不要修改已经运行过的建表语句并指望它生效。那条语句在已有数据库上会被 `IF NOT EXISTS` 跳过。

外键只在连接打开了 `foreign_keys` pragma 时才执行。DSN 里写 `_pragma=foreign_keys(1)`。在一个连接上 `Exec("PRAGMA foreign_keys = ON")` 不够，池里的其他连接没有这个设置。

内存数据库 `:memory:` 配连接池时，每个新连接会看到一个空库。测试因此使用 `t.TempDir()` 里的真实文件。若坚持用内存库，就要 `SetMaxOpenConns(1)`，或使用带共享缓存的具名 DSN，并确认驱动的文档。临时文件更不容易误用。

### 18.3 邮箱、标题、密码

`NormalizeEmail` 去掉空白、改成小写，并做很粗的检查：长度不超过 254，且 `@` 不在两端。这不是 RFC 5322。教程把限制写在函数旁边，避免读者以为它可以替换专门的邮件校验库。

`NormalizeTitle` 按 rune 计数，1 到 200。空白标题拒绝。

`ValidatePassword` 要求 8 到 72 字节。bcrypt 只使用前 72 个字节，更长的密码不会增加搜索空间，还容易让人误以为整段都生效。哈希用 `golang.org/x/crypto/bcrypt` 的 `GenerateFromPassword`，代价用 `DefaultCost`。校验用 `CompareHashAndPassword`。

邮箱不存在时，仍然对一个启动时生成的占位哈希做一次比较。这样「没有这个邮箱」和「密码错误」都花一次 bcrypt 的时间。登录失败统一返回 `ErrInvalid`，HTTP 层写成同一句「邮箱或密码不正确」。不要告诉对方邮箱已经注册在登录接口上。注册接口可以返回冲突，因为对方正是这个邮箱的主人、正在注册。

唯一约束冲突时，驱动返回的错误文本里包含 `UNIQUE`。教程用 `strings.Contains` 识别，并把它映射成 `ErrConflict`。换驱动时要再看错误类型。这是一处明确的简化。

### 18.4 换到 PostgreSQL 时改什么

业务函数的形状不变：`context`、`*sql.DB`、哨兵错误。要改的是这些：

- 驱动换成 `github.com/jackc/pgx/v5/stdlib` 或类似的 PostgreSQL 驱动，`sql.Open` 的名字改为该驱动注册的名字。
- 占位符从 `?` 改成 `$1`、`$2`。可以手写，也可以只在存储层做这个替换。
- `INTEGER PRIMARY KEY` 改成 `BIGINT GENERATED BY DEFAULT AS IDENTITY` 或 `BIGSERIAL`。`LastInsertId` 在部分 PostgreSQL 驱动上不可用，插入改为 `INSERT ... RETURNING id`，用 `QueryRowContext` 扫描编号。
- 布尔列可以直接是 `BOOLEAN`。
- 时间列可以用 `TIMESTAMPTZ`，仍然传 `time.Time`。
- 去掉 SQLite 的 `_pragma`。连接数按数据库配置提高。
- 唯一冲突改成识别 PostgreSQL 的 `23505`，不要再搜索 `UNIQUE` 这三个字母。

HTTP、模板和 JavaScript 不用改。这就是存储层返回普通值和 `error` 的原因。

### 18.5 练习

1. 为什么每个请求里都带 `user_id` 条件，而不是先查出任务再比较？
2. `:memory:` 加上默认连接池，为什么测试会看到空表？
3. 密码超过 72 字节时，bcrypt 实际使用哪一段？

---

## 第 19 章　登录与权限

### 19.1 会话令牌

登录成功时，存储层生成 32 字节的加密随机数，编码成十六进制，写进 `sessions`，过期时间是七天后。这个字符串既放进 Cookie，也作为 API 的 Bearer 令牌。两种入口在中间件里汇合：

1. 若有 `Authorization: Bearer <token>`，用它。
2. 否则读名为 `session` 的 Cookie。
3. 到数据库查找未过期的会话，取出用户。
4. 找不到就按入口返回 401 或 303。

Cookie 的属性：

```go
http.SetCookie(w, &http.Cookie{
    Name:     "session",
    Value:    token,
    Path:     "/",
    HttpOnly: true,
    SameSite: http.SameSiteLaxMode,
    Expires:  time.Now().Add(7 * 24 * time.Hour),
})
```

`HttpOnly` 让页面脚本读不到 Cookie。`SameSite=Lax` 让其他网站发来的普通跨站 POST 不带这个 Cookie。过期时间与数据库里的 `expires_at` 对齐。退出时删除数据库中的会话，并把 Cookie 的 `MaxAge` 设为 -1。

在 HTTPS 上再加 `Secure`。本地 `http://localhost` 不能加，否则浏览器不保存。部署到带证书的域名时加上。

令牌是随机的、服务端可作废的。JWT 把声明签在令牌里，服务端不必保存会话，作废就变难。任务板需要「退出后立刻失效」，所以用服务端会话。JWT 适合已经有统一签发方、而且接受短时有效或黑名单的系统。那是下一步，不是这个程序的结构。

### 19.2 当前用户放在哪里

中间件查到用户后，放进请求的 `Context`。键是未导出的私有类型，包外无法读取或覆盖。处理函数通过 `currentUser` 取出。不要把用户编号放进隐藏表单字段再信任它。编号以会话为准。

```go
type contextKey int

const userKey contextKey = 1
```

两个键都用这个类型的不同常量：一个放用户，一个放令牌。退出时需要令牌本身，才能删除对应的行。

### 19.3 Origin 检查

Cookie 会在同站请求里自动带上。`SameSite=Lax` 已经挡住大部分跨站 POST。教程仍在「用 Cookie 认证、且方法不是 GET/HEAD/OPTIONS」时检查 `Origin` 的主机是否等于请求的 `Host`。对不上就 403。

Bearer 请求不带 Cookie 的自动发送，脚本必须明确加上头部。对这类请求不再做 Origin 比较，否则从 `http://localhost:5173` 发到 `http://localhost:8080` 的合法调用会被拒绝。浏览器仍会先发 CORS 预检。CORS 在第 23 章。

没有 `Origin` 的 Cookie 写请求也拒绝。现代浏览器对 POST 会带上 `Origin`。

### 19.4 密码在页面和 API 上的路径

页面注册：解析表单，`Register`，再 `Login`，设置 Cookie，303 到 `/tasks`。失败就重新渲染表单，邮箱填回输入框，密码不填回。

API 注册：解码 JSON，成功返回 201 和令牌。邮箱冲突返回 409。

登录失败在两条路径上都是同一种结果，不区分邮箱是否存在。

### 19.5 练习

1. `HttpOnly` 和 `SameSite=Lax` 各挡住什么？
2. 为什么退出必须删数据库里的会话，只清 Cookie 不够？
3. 为什么不能相信隐藏表单里的用户编号？

---

## 第 20 章　测试、静态文件与镜像

### 20.1 存储测试

`TestTasksAreScopedToUser` 注册两个人，A 创建任务，B 去标记完成和删除，两者都得到 `ErrNotFound`。A 再列出任务，仍然是未完成的那一条。另一个子断言把 `A@example.com` 再注册一次，得到 `ErrConflict`，确认邮箱按小写比较。

测试数据库放在 `t.TempDir()`。DSN 同样带上外键 pragma。

### 20.2 HTTP 测试

`httptest.NewRecorder` 实现了 `ResponseWriter`。把请求交给 `Server.Handler()`，然后看状态码、Cookie 和正文。

页面流程：

1. `POST /register`，表单里是邮箱和密码，`Origin` 与 `Host` 同为 `example.com`。期望 303，并且有一个 `HttpOnly` 的 `session` Cookie。
2. 带上这个 Cookie `GET /tasks`。正文里能看到邮箱。
3. 带 `HX-Request: true` 提交标题「买牛奶」。正文是一段包含标题的 HTML，而不是整页。
4. 把 `Origin` 改成 `http://evil.example` 再提交。期望 403。

API 流程：

1. `POST /api/register`，`Origin` 为 `http://localhost:5173`。期望 201，以及 CORS 头把这个 Origin 回写。
2. 用返回的令牌 `POST /api/tasks`。
3. `PATCH` 把 `done` 设为 true。
4. 用另一个人的令牌 `DELETE` 这条任务。期望 404。

这些测试不监听端口，不依赖机器上是否有 8080。

### 20.3 模板嵌进二进制

`internal/web` 用 `//go:embed templates/*.html`。测试和 `go build` 都从二进制里读模板，不从当前工作目录读。`template.ParseFS` 在 `New` 里执行。模板语法错误会在启动时失败，而不是等到第一个用户打开页面。

### 20.4 镜像

```dockerfile
FROM golang:1.27 AS build
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .
ENV CGO_ENABLED=0
RUN go build -o /out/taskboard ./cmd/taskboard

FROM gcr.io/distroless/static-debian12
COPY --from=build /out/taskboard /taskboard
ENV TASKBOARD_ADDR=:8080
EXPOSE 8080
ENTRYPOINT ["/taskboard"]
```

第一阶段下载依赖并编译。第二阶段只有可执行文件，没有编译器和 shell。纯 Go 的 SQLite 驱动使 `CGO_ENABLED=0` 成立。数据库文件写在容器的工作目录里时，容器一删文件就没了。正式运行把 SQLite 文件放在挂载的卷上，或改用 PostgreSQL。健康检查可以请求 `GET /healthz`。

构建：

```bash
docker build -t taskboard .
docker run --rm -p 8080:8080 taskboard
```

### 20.5 观测到此为止

请求日志已经够用来学习。`import _ "net/http/pprof"` 会在默认 mux 上注册调试路由。任务板使用自己的 mux，不会自动带上这些路由。需要看泄漏的 goroutine 时，只在本地另挂一个只监听回环地址的 pprof 服务器。不要把 `/debug/pprof` 暴露到公网。它能看出程序内部的调用栈。

### 20.6 练习

1. 为什么 HTTP 测试把 `Host` 和 `Origin` 设成同一个主机名？
2. `embed` 如何避免容器的工作目录里必须带上 `templates/`？
3. 第二阶段镜像里为什么可以不装 Go？

---

## 第 21 章　页面由 Go 渲染

### 21.1 html/template

`html/template` 和 `text/template` 的语法一样，但会按上下文转义。标题放进 HTML 文本时，`<` 会变成 `&lt;`。放进属性时使用另一套转义。因此模板里写 `{{.Title}}`，用户把标题写成一段 `<script>` 时，浏览器看到的是文字，不是脚本。

不要用 `template.HTML` 把用户输入标成「已经安全」。那个类型会跳过转义。任务板的模板里没有它。

`{{define "name"}}` 给一段模板命名。`{{template "name" .}}` 调用它，并把点的数据传进去。点就是当前数据。`{{.Title}}` 读取当前数据的 `Title` 字段。字段必须导出。

```html
{{define "task-item"}}
<li class="task" id="task-{{.ID}}">
  <span class="title{{if .Done}} done{{end}}">{{.Title}}</span>
</li>
{{end}}
```

`{{if .Done}}` 看的是布尔值。`{{range .Tasks}}` 把点换成切片里的每个元素，所以循环内部的 `{{.Title}}` 是任务的标题，不是页面数据的标题。循环外面的点在 `{{end}}` 之后恢复。

### 21.2 页面怎么拼

每个完整页面定义自己的名字：`login-page`、`register-page`、`tasks-page`。它们都调用 `head` 和 `foot`。`head` 里有文档类型、字符集、标题、样式，以及 HTMX 的脚本。样式直接写在头部，任务板不单独维护一套构建后的 CSS 给服务端页面。

渲染：

```go
func (s *Server) render(w http.ResponseWriter, name string, data pageData) {
    w.Header().Set("Content-Type", "text/html; charset=utf-8")
    if err := s.tmpl.ExecuteTemplate(w, name, data); err != nil {
        s.log.Error("template", "name", name, "err", err)
    }
}
```

`pageData` 带上标题、邮箱、错误句子和任务切片。登录页只用标题、邮箱和错误。多出来的空字段保持零值，模板不读取它们。

表单使用普通的 `action` 和 `method`。没有 JavaScript 时，浏览器仍然可以注册、登录、添加和删除。服务器对普通 POST 回应 303。这是第 22 章增强的底。

校验失败时重新渲染整页，错误写在 `<p class="error">` 里，邮箱填回去。密码框留空。

### 21.3 练习

1. 用户把任务标题写成 `<script>alert(1)</script>` 时，`html/template` 会写出什么？
2. `{{range .Tasks}}` 内部的点是什么类型的数据？
3. 为什么密码不在失败后填回输入框？

---

## 第 22 章　HTMX

HTMX 让 HTML 元素自己发出请求，并用响应里的 HTML 替换页面的一块。任务板使用 2.0.4，脚本放在 `head` 里：

```html
<script src="https://unpkg.com/htmx.org@2.0.4"></script>
```

正式环境可以把这个文件下载下来，用 `embed` 从自己的服务器提供。教程用固定版本的 CDN，是为了页面在没有前端构建步骤时就能交互。CDN 不可用时，第 21 章的普通表单仍然能用。

### 22.1 添加任务

```html
<form class="row" action="/tasks" method="post"
      hx-post="/tasks"
      hx-target="#task-list"
      hx-swap="afterbegin"
      hx-on::after-request="if (event.detail.successful && event.detail.target && event.detail.target.id === 'task-list') this.reset()">
  <input type="text" name="title" maxlength="200" required placeholder="要做的事">
  <button type="submit">添加</button>
</form>
<ul class="tasks" id="task-list">
  {{range .Tasks}}{{template "task-item" .}}{{end}}
</ul>
```

`hx-post` 存在时，HTMX 拦截提交，向同一地址发 POST，并加上 `HX-Request: true`。`hx-target="#task-list"` 与 `hx-swap="afterbegin"` 把响应插到列表最前面。服务器若看到 `HX-Request`，就只渲染 `task-item`，不渲染整页。

成功后，`hx-on::after-request` 在目标确实是列表时清空输入框。HTMX 2 用两个冒号表示 `htmx:` 事件。

标题不合法时，服务器返回：

```text
HX-Retarget: #form-error
HX-Reswap: innerHTML
```

正文是一句中文说明。HTMX 按这两个响应头把句子放进错误段落，而不是把句子插进任务列表。目标不再是 `#task-list`，输入框不会被清空。

### 22.2 完成与删除

每个任务是一个 `<li>`。里面两个小表单都有 `action`，所以没有脚本时它们是普通 POST。加上 HTMX 属性之后：

- 切换状态：`hx-target="closest li"`，`hx-swap="outerHTML"`，服务器返回一整个新的 `<li>`。
- 删除：`hx-target="closest li"`，`hx-swap="delete"`，服务器返回空的 200。`delete` 不使用正文。

`closest li` 从按钮往上找到所在的列表项。响应不必包含整页。

### 22.3 服务器怎么分支

```go
func hxRequest(r *http.Request) bool {
    return r.Header.Get("HX-Request") == "true"
}
```

同一个 `POST /tasks` 因此有两种成功结果：片段，或 303。测试分别覆盖它们。不要为 HTMX 再做一套存储函数。

### 22.4 练习

1. 没有 HTMX 脚本时，添加任务的表单还会提交吗？提交后服务器返回什么？
2. `HX-Retarget` 解决了校验失败时的什么问题？
3. 删除为什么可以返回空正文？

---

## 第 23 章　给浏览器的 API 契约

第 24 章的页面跑在 Vite 的开发服务器上，地址是 `http://localhost:5173`。API 在 `http://localhost:8080`。浏览器把这看成两个源。脚本要读响应，服务器就必须在 CORS 里明确允许这个源。

### 23.1 允许的源

```go
func allowedOrigin(r *http.Request) string {
    switch r.Header.Get("Origin") {
    case "http://localhost:5173", "http://127.0.0.1:5173":
        return r.Header.Get("Origin")
    default:
        return ""
    }
}
```

匹配时回写三个头：

```text
Access-Control-Allow-Origin: http://localhost:5173
Vary: Origin
Access-Control-Allow-Headers: Content-Type, Authorization
Access-Control-Allow-Methods: GET, POST, PATCH, DELETE, OPTIONS
```

`Allow-Origin` 不要写成 `*` 再同时允许带凭证的请求。任务板的 API 使用 Bearer 头部，不使用跨站 Cookie，所以不设置 `Access-Control-Allow-Credentials`。令牌放在 `sessionStorage`，由脚本自己附上。

预检是浏览器在真正的 `POST` 之前自动发出的 `OPTIONS`。它没有业务意义。处理函数外包一层：

```go
if origin := allowedOrigin(r); origin != "" {
    // 设置上面的三个头
}
if r.Method == http.MethodOptions {
    w.WriteHeader(http.StatusNoContent)
    return
}
```

这层放在 chi 之外，预检不必在每条路由上注册。不允许的源不会拿到 `Allow-Origin`，浏览器会拦住脚本读取响应。服务器仍会执行请求。CORS 不是认证。认证是 Bearer 令牌。

### 23.2 契约

| 方法与路径 | 正文 | 成功 | 失败 |
| --- | --- | --- | --- |
| `POST /api/register` | `email`、`password` | 201 与 `token` | 400 或 409 |
| `POST /api/login` | 同上 | 200 与 `token` | 401 |
| `GET /api/tasks` | 无 | 200 与任务数组 | 401 |
| `POST /api/tasks` | `title` | 201 与任务 | 400 或 401 |
| `PATCH /api/tasks/{id}` | `title` 或 `done` 或两者 | 200 与任务 | 400、401、404 |
| `DELETE /api/tasks/{id}` | 无 | 204 | 401 或 404 |
| `POST /api/logout` | 无 | 204 | 401 |

时间字段是 UTC。编号是 JSON 数字。客户端不要自己拼 SQL 或假设编号连续。

用浏览器的开发者工具看网络面板：注册应先出现 `OPTIONS`，再出现 `POST`。`POST` 的响应头里有 `Access-Control-Allow-Origin`。令牌出现在随后请求的 `Authorization` 头里，不出现在 URL 里。

### 23.3 练习

1. CORS 允许了一个源之后，没有令牌的 `GET /api/tasks` 应该返回什么？
2. 为什么预检处理放在路由外面？
3. 为什么不把令牌放在查询字符串里？

---

## 第 24 章　Vite 与原生 JavaScript

这一章的页面只做四件事：注册或登录、列出任务、添加、切换完成或删除。不引入 React 或 Vue。DOM 由一个 `render` 函数根据状态重新画出来。

### 24.1 状态

```js
const state = {
  token: sessionStorage.getItem("token") || "",
  email: sessionStorage.getItem("email") || "",
  tasks: [],
  error: "",
  mode: "login",
};
```

`sessionStorage` 在这个标签页里保持登录。关掉标签页就没了。它比 `localStorage` 的寿命短，适合这种练习。令牌仍然是一串能调用 API 的秘密，不要写进日志，不要放进会同步到服务器的分析脚本。

### 24.2 请求

`request` 设置 `Content-Type` 和 `Authorization`，然后 `fetch`。204 没有正文，函数返回 `null`，不再调用 `response.json()`。失败时抛出 `Error`，消息来自 JSON 里的 `error` 字段。界面把 `state.error` 显示在段落里。

所有写进 `innerHTML` 的任务标题都经过 `escapeHTML`。这里的页面不是 `html/template`，不会自动转义。漏掉这一步，标题里的 `<` 就会变成标签。

### 24.3 事件

每次 `render` 之后重新绑定事件，因为旧的按钮已经不在文档里。登录成功就把令牌写入 `sessionStorage` 并调用 `loadTasks`。退出先请求 `POST /api/logout`，再清本地状态。即使退出请求失败，本地也清掉令牌，下一次请求不会再带上它。服务端那一行若删除失败，会等到过期。练习项目接受这一点，并在界面上把错误留下来。

`loadTasks` 若发现错误文字里有 `login`，就清掉本地令牌。401 的正文是 `login required`。这是教学用的判断。更稳的做法是看 `response.status === 401`，把状态码从 `request` 里传出来。你在练习里可以改成状态码。

### 24.4 运行

在 `frontend` 目录：

```bash
npm install
npm run dev
```

Vite 默认打开 5173。API 进程另开一个终端：

```bash
go run ./cmd/taskboard
```

先注册，再添加一条中文标题，刷新页面，任务还在。再到 SQLite 里看 `tasks` 表，同一行会被服务端页面看见。两个界面共用数据库。

`package.json` 只依赖 Vite，用来提供开发服务器和模块脚本。没有框架运行时。

### 24.5 练习

1. 为什么这个前端必须自己做 HTML 转义？
2. 204 响应为什么不能调用 `response.json()`？
3. 把「错误文字包含 login」改成判断状态码，`request` 函数要多返回什么？

---

## 第 25 章　任务板

把前面的存储、HTTP、模板和前端放在一个模块里，得到可以运行的任务板。模块路径是 `example.com/taskboard`。下面的文件与已经通过 `go test ./...` 的源码一致。Go 版本是 1.27。直接依赖是 chi v5.2.3、`golang.org/x/crypto` v0.41.0、`modernc.org/sqlite` v1.38.2。

目录：

```text
taskboard/
  go.mod
  Dockerfile
  cmd/taskboard/main.go
  internal/store/store.go
  internal/store/store_test.go
  internal/web/server.go
  internal/web/server_test.go
  internal/web/templates/head.html
  internal/web/templates/auth.html
  internal/web/templates/tasks.html
  frontend/index.html
  frontend/main.js
  frontend/styles.css
  frontend/package.json
```

本地运行：

```bash
go test ./...
go run ./cmd/taskboard
```

浏览器打开 `http://localhost:8080/register` 使用服务端页面。另开 `frontend` 的 Vite 使用 JSON API。两个界面看到同一份 SQLite 数据。

源码如下。

### go.mod

```text
module example.com/taskboard

go 1.27

require (
	github.com/go-chi/chi/v5 v5.2.3
	golang.org/x/crypto v0.41.0
	modernc.org/sqlite v1.38.2
)

require (
	github.com/dustin/go-humanize v1.0.1 // indirect
	github.com/google/uuid v1.6.0 // indirect
	github.com/mattn/go-isatty v0.0.20 // indirect
	github.com/ncruces/go-strftime v0.1.9 // indirect
	github.com/remyoudompheng/bigfft v0.0.0-20230129092748-24d4a6f8daec // indirect
	golang.org/x/exp v0.0.0-20250620022241-b7579e27df2b // indirect
	golang.org/x/sys v0.35.0 // indirect
	modernc.org/libc v1.66.3 // indirect
	modernc.org/mathutil v1.7.1 // indirect
	modernc.org/memory v1.11.0 // indirect
)
```


### cmd/taskboard/main.go

```go
package main

import (
	"context"
	"errors"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"example.com/taskboard/internal/store"
	"example.com/taskboard/internal/web"
)

func main() {
	log := slog.New(slog.NewTextHandler(os.Stdout, nil))

	dsn := os.Getenv("TASKBOARD_DSN")
	if dsn == "" {
		dsn = "file:taskboard.db?_pragma=foreign_keys(1)&_pragma=busy_timeout(5000)"
	}
	addr := os.Getenv("TASKBOARD_ADDR")
	if addr == "" {
		addr = ":8080"
	}

	db, err := store.Open(dsn)
	if err != nil {
		log.Error("open database", "err", err)
		os.Exit(1)
	}
	defer db.Close()

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	err = store.Migrate(ctx, db)
	cancel()
	if err != nil {
		log.Error("migrate", "err", err)
		os.Exit(1)
	}

	app, err := web.New(db, log)
	if err != nil {
		log.Error("templates", "err", err)
		os.Exit(1)
	}

	srv := &http.Server{
		Addr:              addr,
		Handler:           app.Handler(),
		ReadHeaderTimeout: 5 * time.Second,
	}

	go func() {
		log.Info("listen", "addr", addr)
		if err := srv.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
			log.Error("serve", "err", err)
			os.Exit(1)
		}
	}()

	stop, stopCancel := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer stopCancel()
	<-stop.Done()

	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer shutdownCancel()
	if err := srv.Shutdown(shutdownCtx); err != nil {
		log.Error("shutdown", "err", err)
		os.Exit(1)
	}
}
```


### internal/store/store.go

```go
// Package store 用 database/sql 把用户、会话和任务放进 SQLite。
package store

import (
	"context"
	"crypto/rand"
	"database/sql"
	"encoding/hex"
	"errors"
	"fmt"
	"strings"
	"time"

	"golang.org/x/crypto/bcrypt"
	_ "modernc.org/sqlite"
)

var (
	// ErrInvalid 表示输入不符合规则，或邮箱与密码不匹配。
	ErrInvalid = errors.New("invalid")
	// ErrConflict 表示邮箱已被注册。
	ErrConflict = errors.New("conflict")
	// ErrNotFound 表示记录不存在，或不属于当前用户。
	ErrNotFound = errors.New("not found")
)

// User 是已注册的人。PasswordHash 只留在服务端。
type User struct {
	ID           int64
	Email        string
	PasswordHash string
}

// Task 是一张任务。JSON 名字给浏览器脚本用，页面模板用同一批字段。
type Task struct {
	ID        int64     `json:"id"`
	Title     string    `json:"title"`
	Done      bool      `json:"done"`
	CreatedAt time.Time `json:"createdAt"`
}

// Open 打开 SQLite。dsn 由调用方带上 foreign_keys 等 pragma。
func Open(dsn string) (*sql.DB, error) {
	db, err := sql.Open("sqlite", dsn)
	if err != nil {
		return nil, fmt.Errorf("open sqlite: %w", err)
	}
	db.SetMaxOpenConns(8)
	db.SetMaxIdleConns(8)
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	if err := db.PingContext(ctx); err != nil {
		db.Close()
		return nil, fmt.Errorf("ping sqlite: %w", err)
	}
	return db, nil
}

// Migrate 创建还没有的表。重复执行是安全的。
func Migrate(ctx context.Context, db *sql.DB) error {
	_, err := db.ExecContext(ctx, `
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY,
    email TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    created_at TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS sessions (
    token TEXT PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id),
    expires_at TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS tasks (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id),
    title TEXT NOT NULL,
    done INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL
);`)
	if err != nil {
		return fmt.Errorf("migrate: %w", err)
	}
	return nil
}

// NormalizeEmail 去掉首尾空白并改为小写。这里只做教学用的粗校验。
func NormalizeEmail(s string) (string, error) {
	s = strings.ToLower(strings.TrimSpace(s))
	if len(s) > 254 || !strings.Contains(s, "@") || strings.HasPrefix(s, "@") || strings.HasSuffix(s, "@") {
		return "", ErrInvalid
	}
	return s, nil
}

// NormalizeTitle 按 Unicode 字符计数，上限 200。len(s) 数的是字节，不能拿来限制标题。
func NormalizeTitle(s string) (string, error) {
	s = strings.TrimSpace(s)
	n := len([]rune(s))
	if n == 0 || n > 200 {
		return "", ErrInvalid
	}
	return s, nil
}

// ValidatePassword 限制在 bcrypt 会完整使用的长度里。bcrypt 只取前 72 个字节。
func ValidatePassword(s string) error {
	if len(s) < 8 || len(s) > 72 {
		return ErrInvalid
	}
	return nil
}

// Register 创建用户。邮箱重复时返回 ErrConflict。
func Register(ctx context.Context, db *sql.DB, email, password string) (User, error) {
	email, err := NormalizeEmail(email)
	if err != nil {
		return User{}, err
	}
	if err := ValidatePassword(password); err != nil {
		return User{}, err
	}
	hash, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	if err != nil {
		return User{}, fmt.Errorf("hash password: %w", err)
	}
	now := time.Now().UTC().Format(time.RFC3339Nano)
	res, err := db.ExecContext(ctx,
		`INSERT INTO users(email, password_hash, created_at) VALUES (?, ?, ?)`,
		email, string(hash), now)
	if err != nil {
		if strings.Contains(err.Error(), "UNIQUE") {
			return User{}, ErrConflict
		}
		return User{}, fmt.Errorf("insert user: %w", err)
	}
	id, err := res.LastInsertId()
	if err != nil {
		return User{}, fmt.Errorf("user id: %w", err)
	}
	return User{ID: id, Email: email, PasswordHash: string(hash)}, nil
}

// dummyHash 让「没有这个邮箱」和「密码错误」走同样的 bcrypt 比较，避免用响应时间枚举邮箱。
var dummyHash []byte

func init() {
	var err error
	dummyHash, err = bcrypt.GenerateFromPassword([]byte("not-a-real-password"), bcrypt.DefaultCost)
	if err != nil {
		panic(err)
	}
}

// Login 核对邮箱和密码，成功时写下新会话并返回令牌。
func Login(ctx context.Context, db *sql.DB, email, password string) (User, string, error) {
	email, err := NormalizeEmail(email)
	if err != nil {
		return User{}, "", err
	}
	user, findErr := userByEmail(ctx, db, email)
	hash := dummyHash
	if findErr == nil {
		hash = []byte(user.PasswordHash)
	}
	if findErr != nil || bcrypt.CompareHashAndPassword(hash, []byte(password)) != nil {
		return User{}, "", ErrInvalid
	}
	token, err := newToken()
	if err != nil {
		return User{}, "", err
	}
	expires := time.Now().UTC().Add(7 * 24 * time.Hour).Format(time.RFC3339Nano)
	if _, err := db.ExecContext(ctx,
		`INSERT INTO sessions(token, user_id, expires_at) VALUES (?, ?, ?)`,
		token, user.ID, expires); err != nil {
		return User{}, "", fmt.Errorf("insert session: %w", err)
	}
	return user, token, nil
}

func userByEmail(ctx context.Context, db *sql.DB, email string) (User, error) {
	var u User
	err := db.QueryRowContext(ctx,
		`SELECT id, email, password_hash FROM users WHERE email = ?`, email).
		Scan(&u.ID, &u.Email, &u.PasswordHash)
	if errors.Is(err, sql.ErrNoRows) {
		return User{}, ErrNotFound
	}
	if err != nil {
		return User{}, fmt.Errorf("select user: %w", err)
	}
	return u, nil
}

func newToken() (string, error) {
	buf := make([]byte, 32)
	if _, err := rand.Read(buf); err != nil {
		return "", fmt.Errorf("session token: %w", err)
	}
	return hex.EncodeToString(buf), nil
}

// UserByToken 读取未过期的会话。过期会话会被删掉。
func UserByToken(ctx context.Context, db *sql.DB, token string) (User, error) {
	var u User
	var expires string
	err := db.QueryRowContext(ctx, `
SELECT u.id, u.email, u.password_hash, s.expires_at
FROM sessions s JOIN users u ON u.id = s.user_id
WHERE s.token = ?`, token).Scan(&u.ID, &u.Email, &u.PasswordHash, &expires)
	if errors.Is(err, sql.ErrNoRows) {
		return User{}, ErrNotFound
	}
	if err != nil {
		return User{}, fmt.Errorf("select session: %w", err)
	}
	deadline, err := time.Parse(time.RFC3339Nano, expires)
	if err != nil || !time.Now().Before(deadline) {
		_, _ = db.ExecContext(ctx, `DELETE FROM sessions WHERE token = ?`, token)
		return User{}, ErrNotFound
	}
	return u, nil
}

// Logout 删除会话。令牌本来就不存在时也算成功。
func Logout(ctx context.Context, db *sql.DB, token string) error {
	if _, err := db.ExecContext(ctx, `DELETE FROM sessions WHERE token = ?`, token); err != nil {
		return fmt.Errorf("delete session: %w", err)
	}
	return nil
}

// CreateTask 为该用户新增一条未完成任务。
func CreateTask(ctx context.Context, db *sql.DB, userID int64, title string) (Task, error) {
	title, err := NormalizeTitle(title)
	if err != nil {
		return Task{}, err
	}
	now := time.Now().UTC()
	res, err := db.ExecContext(ctx,
		`INSERT INTO tasks(user_id, title, done, created_at) VALUES (?, ?, 0, ?)`,
		userID, title, now.Format(time.RFC3339Nano))
	if err != nil {
		return Task{}, fmt.Errorf("insert task: %w", err)
	}
	id, err := res.LastInsertId()
	if err != nil {
		return Task{}, fmt.Errorf("task id: %w", err)
	}
	return Task{ID: id, Title: title, Done: false, CreatedAt: now}, nil
}

// ListTasks 返回该用户的任务，新的在前。
func ListTasks(ctx context.Context, db *sql.DB, userID int64) ([]Task, error) {
	rows, err := db.QueryContext(ctx, `
SELECT id, title, done, created_at FROM tasks
WHERE user_id = ? ORDER BY created_at DESC, id DESC`, userID)
	if err != nil {
		return nil, fmt.Errorf("list tasks: %w", err)
	}
	defer rows.Close()
	var out []Task
	for rows.Next() {
		t, err := scanTask(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, t)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("list tasks: %w", err)
	}
	return out, nil
}

type scanner interface {
	Scan(dest ...any) error
}

func scanTask(s scanner) (Task, error) {
	var t Task
	var done int
	var created string
	if err := s.Scan(&t.ID, &t.Title, &done, &created); err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			return Task{}, ErrNotFound
		}
		return Task{}, fmt.Errorf("scan task: %w", err)
	}
	t.Done = done != 0
	parsed, err := time.Parse(time.RFC3339Nano, created)
	if err != nil {
		return Task{}, fmt.Errorf("parse created_at: %w", err)
	}
	t.CreatedAt = parsed
	return t, nil
}

// ToggleDone 在完成与未完成之间切换。
func ToggleDone(ctx context.Context, db *sql.DB, userID, taskID int64) (Task, error) {
	res, err := db.ExecContext(ctx, `
UPDATE tasks
SET done = CASE done WHEN 0 THEN 1 ELSE 0 END
WHERE id = ? AND user_id = ?`, taskID, userID)
	if err != nil {
		return Task{}, fmt.Errorf("toggle task: %w", err)
	}
	n, err := res.RowsAffected()
	if err != nil {
		return Task{}, fmt.Errorf("toggle task: %w", err)
	}
	if n == 0 {
		return Task{}, ErrNotFound
	}
	return taskByID(ctx, db, userID, taskID)
}

// SetDone 把完成状态设成给定值。任务不属于该用户时返回 ErrNotFound。
func SetDone(ctx context.Context, db *sql.DB, userID, taskID int64, done bool) (Task, error) {
	flag := 0
	if done {
		flag = 1
	}
	res, err := db.ExecContext(ctx,
		`UPDATE tasks SET done = ? WHERE id = ? AND user_id = ?`, flag, taskID, userID)
	if err != nil {
		return Task{}, fmt.Errorf("update task: %w", err)
	}
	n, err := res.RowsAffected()
	if err != nil {
		return Task{}, fmt.Errorf("update task: %w", err)
	}
	if n == 0 {
		return Task{}, ErrNotFound
	}
	return taskByID(ctx, db, userID, taskID)
}

// UpdateTitle 修改标题。
func UpdateTitle(ctx context.Context, db *sql.DB, userID, taskID int64, title string) (Task, error) {
	title, err := NormalizeTitle(title)
	if err != nil {
		return Task{}, err
	}
	res, err := db.ExecContext(ctx,
		`UPDATE tasks SET title = ? WHERE id = ? AND user_id = ?`, title, taskID, userID)
	if err != nil {
		return Task{}, fmt.Errorf("rename task: %w", err)
	}
	n, err := res.RowsAffected()
	if err != nil {
		return Task{}, fmt.Errorf("rename task: %w", err)
	}
	if n == 0 {
		return Task{}, ErrNotFound
	}
	return taskByID(ctx, db, userID, taskID)
}

func taskByID(ctx context.Context, db *sql.DB, userID, taskID int64) (Task, error) {
	row := db.QueryRowContext(ctx, `
SELECT id, title, done, created_at FROM tasks WHERE id = ? AND user_id = ?`, taskID, userID)
	return scanTask(row)
}

// DeleteTask 删除任务。不属于该用户时返回 ErrNotFound。
func DeleteTask(ctx context.Context, db *sql.DB, userID, taskID int64) error {
	res, err := db.ExecContext(ctx,
		`DELETE FROM tasks WHERE id = ? AND user_id = ?`, taskID, userID)
	if err != nil {
		return fmt.Errorf("delete task: %w", err)
	}
	n, err := res.RowsAffected()
	if err != nil {
		return fmt.Errorf("delete task: %w", err)
	}
	if n == 0 {
		return ErrNotFound
	}
	return nil
}
```


### internal/store/store_test.go

```go
package store

import (
	"context"
	"database/sql"
	"errors"
	"path/filepath"
	"strings"
	"testing"
)

func TestNormalizeTitleCountsRunes(t *testing.T) {
	got, err := NormalizeTitle("  买牛奶  ")
	if err != nil || got != "买牛奶" {
		t.Fatalf("got %q %v", got, err)
	}
	if _, err := NormalizeTitle("   "); !errors.Is(err, ErrInvalid) {
		t.Fatalf("blank: %v", err)
	}
	if _, err := NormalizeTitle(strings.Repeat("题", 201)); !errors.Is(err, ErrInvalid) {
		t.Fatalf("long: %v", err)
	}
}

func TestTasksAreScopedToUser(t *testing.T) {
	ctx := context.Background()
	db := openTestDB(t)
	a, err := Register(ctx, db, "a@example.com", "password-a")
	if err != nil {
		t.Fatal(err)
	}
	b, err := Register(ctx, db, "b@example.com", "password-b")
	if err != nil {
		t.Fatal(err)
	}
	task, err := CreateTask(ctx, db, a.ID, "写教程")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := SetDone(ctx, db, b.ID, task.ID, true); !errors.Is(err, ErrNotFound) {
		t.Fatalf("other user set done: %v", err)
	}
	if err := DeleteTask(ctx, db, b.ID, task.ID); !errors.Is(err, ErrNotFound) {
		t.Fatalf("other user delete: %v", err)
	}
	got, err := ListTasks(ctx, db, a.ID)
	if err != nil || len(got) != 1 || got[0].Done {
		t.Fatalf("owner list: %+v %v", got, err)
	}
	if _, err := Register(ctx, db, "A@example.com", "password-a"); !errors.Is(err, ErrConflict) {
		t.Fatalf("duplicate email: %v", err)
	}
}

func openTestDB(t *testing.T) *sql.DB {
	t.Helper()
	dsn := "file:" + filepath.Join(t.TempDir(), "test.db") + "?_pragma=foreign_keys(1)"
	db, err := Open(dsn)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { db.Close() })
	if err := Migrate(context.Background(), db); err != nil {
		t.Fatal(err)
	}
	return db
}
```


### internal/web/server.go

```go
// Package web 提供页面、HTMX 片段和 JSON API。
package web

import (
	"context"
	"database/sql"
	"embed"
	"encoding/json"
	"errors"
	"html/template"
	"io"
	"log/slog"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"time"

	"example.com/taskboard/internal/store"
	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"
)

//go:embed templates/*.html
var templateFS embed.FS

type contextKey int

const userKey contextKey = 1

// Server 持有数据库和日志器。处理函数都挂在它上面。
type Server struct {
	db   *sql.DB
	log  *slog.Logger
	tmpl *template.Template
}

// New 解析模板并返回服务器。
func New(db *sql.DB, log *slog.Logger) (*Server, error) {
	tmpl, err := template.ParseFS(templateFS, "templates/*.html")
	if err != nil {
		return nil, err
	}
	return &Server{db: db, log: log, tmpl: tmpl}, nil
}

// Handler 在 chi 路由外处理 CORS 预检，再进入路由。
func (s *Server) Handler() http.Handler {
	routes := s.routes()
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if origin := allowedOrigin(r); origin != "" {
			w.Header().Set("Access-Control-Allow-Origin", origin)
			w.Header().Set("Vary", "Origin")
			w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization")
			w.Header().Set("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS")
		}
		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusNoContent)
			return
		}
		routes.ServeHTTP(w, r)
	})
}

func allowedOrigin(r *http.Request) string {
	switch r.Header.Get("Origin") {
	case "http://localhost:5173", "http://127.0.0.1:5173":
		return r.Header.Get("Origin")
	default:
		return ""
	}
}

func (s *Server) routes() http.Handler {
	r := chi.NewRouter()
	r.Use(middleware.RequestID)
	r.Use(middleware.Recoverer)
	r.Use(s.logRequests)

	r.Get("/healthz", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte("ok"))
	})
	r.Get("/", func(w http.ResponseWriter, r *http.Request) {
		http.Redirect(w, r, "/tasks", http.StatusSeeOther)
	})
	r.Get("/login", s.loginPage)
	r.Post("/login", s.login)
	r.Get("/register", s.registerPage)
	r.Post("/register", s.register)

	r.Group(func(r chi.Router) {
		r.Use(s.requireUser)
		r.Get("/tasks", s.tasksPage)
		r.Post("/tasks", s.createTask)
		r.Post("/tasks/{id}/toggle", s.toggleTask)
		r.Post("/tasks/{id}/delete", s.deleteTask)
		r.Post("/logout", s.logout)
	})

	r.Route("/api", func(r chi.Router) {
		r.Post("/register", s.apiRegister)
		r.Post("/login", s.apiLogin)
		r.Group(func(r chi.Router) {
			r.Use(s.requireUser)
			r.Get("/tasks", s.apiList)
			r.Post("/tasks", s.apiCreate)
			r.Patch("/tasks/{id}", s.apiPatch)
			r.Delete("/tasks/{id}", s.apiDelete)
			r.Post("/logout", s.apiLogout)
		})
	})
	return r
}

func (s *Server) logRequests(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		ww := middleware.NewWrapResponseWriter(w, r.ProtoMajor)
		next.ServeHTTP(ww, r)
		s.log.Info("request",
			"method", r.Method,
			"path", r.URL.Path,
			"status", ww.Status(),
			"dur", time.Since(start))
	})
}

func (s *Server) requireUser(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		token, via := sessionToken(r)
		if token == "" {
			s.unauthenticated(w, r)
			return
		}
		user, err := store.UserByToken(r.Context(), s.db, token)
		if err != nil {
			s.unauthenticated(w, r)
			return
		}
		if via == "cookie" && !safeMethod(r.Method) && !sameOrigin(r) {
			http.Error(w, "bad origin", http.StatusForbidden)
			return
		}
		ctx := context.WithValue(r.Context(), userKey, user)
		ctx = context.WithValue(ctx, tokenKey, token)
		next.ServeHTTP(w, r.WithContext(ctx))
	})
}

const tokenKey contextKey = 2

func sessionToken(r *http.Request) (string, string) {
	if h := r.Header.Get("Authorization"); strings.HasPrefix(h, "Bearer ") {
		return strings.TrimSpace(strings.TrimPrefix(h, "Bearer ")), "bearer"
	}
	c, err := r.Cookie("session")
	if err != nil || c.Value == "" {
		return "", ""
	}
	return c.Value, "cookie"
}

func safeMethod(method string) bool {
	switch method {
	case http.MethodGet, http.MethodHead, http.MethodOptions:
		return true
	default:
		return false
	}
}

func sameOrigin(r *http.Request) bool {
	origin := r.Header.Get("Origin")
	if origin == "" {
		return false
	}
	u, err := url.Parse(origin)
	if err != nil {
		return false
	}
	return u.Host == r.Host
}

func currentUser(ctx context.Context) store.User {
	user, _ := ctx.Value(userKey).(store.User)
	return user
}

func currentToken(ctx context.Context) string {
	token, _ := ctx.Value(tokenKey).(string)
	return token
}

func (s *Server) unauthenticated(w http.ResponseWriter, r *http.Request) {
	if strings.HasPrefix(r.URL.Path, "/api/") {
		writeJSON(w, http.StatusUnauthorized, map[string]string{"error": "login required"})
		return
	}
	http.Redirect(w, r, "/login", http.StatusSeeOther)
}

func setSessionCookie(w http.ResponseWriter, token string) {
	http.SetCookie(w, &http.Cookie{
		Name:     "session",
		Value:    token,
		Path:     "/",
		HttpOnly: true,
		SameSite: http.SameSiteLaxMode,
		Expires:  time.Now().Add(7 * 24 * time.Hour),
	})
}

func clearSessionCookie(w http.ResponseWriter) {
	http.SetCookie(w, &http.Cookie{
		Name:     "session",
		Value:    "",
		Path:     "/",
		HttpOnly: true,
		SameSite: http.SameSiteLaxMode,
		MaxAge:   -1,
	})
}

type pageData struct {
	Title string
	Email string
	Error string
	Tasks []store.Task
}

func (s *Server) render(w http.ResponseWriter, name string, data pageData) {
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	if err := s.tmpl.ExecuteTemplate(w, name, data); err != nil {
		s.log.Error("template", "name", name, "err", err)
	}
}

func (s *Server) loginPage(w http.ResponseWriter, r *http.Request) {
	s.render(w, "login-page", pageData{Title: "登录"})
}

func (s *Server) registerPage(w http.ResponseWriter, r *http.Request) {
	s.render(w, "register-page", pageData{Title: "注册"})
}

func (s *Server) login(w http.ResponseWriter, r *http.Request) {
	if err := r.ParseForm(); err != nil {
		http.Error(w, "bad form", http.StatusBadRequest)
		return
	}
	email := r.PostForm.Get("email")
	user, token, err := store.Login(r.Context(), s.db, email, r.PostForm.Get("password"))
	if err != nil {
		s.render(w, "login-page", pageData{Title: "登录", Email: email, Error: "邮箱或密码不正确"})
		return
	}
	setSessionCookie(w, token)
	_ = user
	http.Redirect(w, r, "/tasks", http.StatusSeeOther)
}

func (s *Server) register(w http.ResponseWriter, r *http.Request) {
	if err := r.ParseForm(); err != nil {
		http.Error(w, "bad form", http.StatusBadRequest)
		return
	}
	email := r.PostForm.Get("email")
	password := r.PostForm.Get("password")
	if _, err := store.Register(r.Context(), s.db, email, password); err != nil {
		msg := "注册信息不正确"
		if errors.Is(err, store.ErrConflict) {
			msg = "这个邮箱已经注册"
		}
		s.render(w, "register-page", pageData{Title: "注册", Email: email, Error: msg})
		return
	}
	_, token, err := store.Login(r.Context(), s.db, email, password)
	if err != nil {
		http.Redirect(w, r, "/login", http.StatusSeeOther)
		return
	}
	setSessionCookie(w, token)
	http.Redirect(w, r, "/tasks", http.StatusSeeOther)
}

func (s *Server) tasksPage(w http.ResponseWriter, r *http.Request) {
	user := currentUser(r.Context())
	tasks, err := store.ListTasks(r.Context(), s.db, user.ID)
	if err != nil {
		http.Error(w, "list failed", http.StatusInternalServerError)
		return
	}
	s.render(w, "tasks-page", pageData{Title: "任务", Email: user.Email, Tasks: tasks})
}

func (s *Server) createTask(w http.ResponseWriter, r *http.Request) {
	if err := r.ParseForm(); err != nil {
		http.Error(w, "bad form", http.StatusBadRequest)
		return
	}
	user := currentUser(r.Context())
	task, err := store.CreateTask(r.Context(), s.db, user.ID, r.PostForm.Get("title"))
	if err != nil {
		if hxRequest(r) {
			w.Header().Set("HX-Retarget", "#form-error")
			w.Header().Set("HX-Reswap", "innerHTML")
			_, _ = w.Write([]byte("标题需要 1 到 200 个字符"))
			return
		}
		tasks, _ := store.ListTasks(r.Context(), s.db, user.ID)
		s.render(w, "tasks-page", pageData{Title: "任务", Email: user.Email, Tasks: tasks, Error: "标题需要 1 到 200 个字符"})
		return
	}
	if hxRequest(r) {
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		if err := s.tmpl.ExecuteTemplate(w, "task-item", task); err != nil {
			s.log.Error("template", "err", err)
		}
		return
	}
	http.Redirect(w, r, "/tasks", http.StatusSeeOther)
}

func (s *Server) toggleTask(w http.ResponseWriter, r *http.Request) {
	user := currentUser(r.Context())
	id, err := pathID(r)
	if err != nil {
		http.NotFound(w, r)
		return
	}
	task, err := store.ToggleDone(r.Context(), s.db, user.ID, id)
	if err != nil {
		http.NotFound(w, r)
		return
	}
	if hxRequest(r) {
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		_ = s.tmpl.ExecuteTemplate(w, "task-item", task)
		return
	}
	http.Redirect(w, r, "/tasks", http.StatusSeeOther)
}

func (s *Server) deleteTask(w http.ResponseWriter, r *http.Request) {
	user := currentUser(r.Context())
	id, err := pathID(r)
	if err != nil {
		http.NotFound(w, r)
		return
	}
	if err := store.DeleteTask(r.Context(), s.db, user.ID, id); err != nil {
		http.NotFound(w, r)
		return
	}
	if hxRequest(r) {
		w.WriteHeader(http.StatusOK)
		return
	}
	http.Redirect(w, r, "/tasks", http.StatusSeeOther)
}

func (s *Server) logout(w http.ResponseWriter, r *http.Request) {
	_ = store.Logout(r.Context(), s.db, currentToken(r.Context()))
	clearSessionCookie(w)
	http.Redirect(w, r, "/login", http.StatusSeeOther)
}

func hxRequest(r *http.Request) bool {
	return r.Header.Get("HX-Request") == "true"
}

func pathID(r *http.Request) (int64, error) {
	return strconv.ParseInt(chi.URLParam(r, "id"), 10, 64)
}

type credentials struct {
	Email    string `json:"email"`
	Password string `json:"password"`
}

type tokenResponse struct {
	Token string `json:"token"`
}

type taskInput struct {
	Title *string `json:"title"`
	Done  *bool   `json:"done"`
}

func (s *Server) apiRegister(w http.ResponseWriter, r *http.Request) {
	var body credentials
	if err := decodeJSON(w, r, &body); err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "invalid json"})
		return
	}
	if _, err := store.Register(r.Context(), s.db, body.Email, body.Password); err != nil {
		status := http.StatusBadRequest
		msg := "invalid email or password"
		if errors.Is(err, store.ErrConflict) {
			status = http.StatusConflict
			msg = "email already registered"
		}
		writeJSON(w, status, map[string]string{"error": msg})
		return
	}
	_, token, err := store.Login(r.Context(), s.db, body.Email, body.Password)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "login after register failed"})
		return
	}
	writeJSON(w, http.StatusCreated, tokenResponse{Token: token})
}

func (s *Server) apiLogin(w http.ResponseWriter, r *http.Request) {
	var body credentials
	if err := decodeJSON(w, r, &body); err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "invalid json"})
		return
	}
	_, token, err := store.Login(r.Context(), s.db, body.Email, body.Password)
	if err != nil {
		writeJSON(w, http.StatusUnauthorized, map[string]string{"error": "invalid email or password"})
		return
	}
	writeJSON(w, http.StatusOK, tokenResponse{Token: token})
}

func (s *Server) apiList(w http.ResponseWriter, r *http.Request) {
	tasks, err := store.ListTasks(r.Context(), s.db, currentUser(r.Context()).ID)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "list failed"})
		return
	}
	if tasks == nil {
		tasks = []store.Task{}
	}
	writeJSON(w, http.StatusOK, tasks)
}

func (s *Server) apiCreate(w http.ResponseWriter, r *http.Request) {
	var body taskInput
	if err := decodeJSON(w, r, &body); err != nil || body.Title == nil {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "title is required"})
		return
	}
	task, err := store.CreateTask(r.Context(), s.db, currentUser(r.Context()).ID, *body.Title)
	if errors.Is(err, store.ErrInvalid) {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "title must be 1 to 200 characters"})
		return
	}
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "create failed"})
		return
	}
	writeJSON(w, http.StatusCreated, task)
}

func (s *Server) apiPatch(w http.ResponseWriter, r *http.Request) {
	id, err := pathID(r)
	if err != nil {
		writeJSON(w, http.StatusNotFound, map[string]string{"error": "not found"})
		return
	}
	var body taskInput
	if err := decodeJSON(w, r, &body); err != nil || (body.Title == nil && body.Done == nil) {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "nothing to update"})
		return
	}
	userID := currentUser(r.Context()).ID
	if body.Title != nil {
		if _, err := store.UpdateTitle(r.Context(), s.db, userID, id, *body.Title); err != nil {
			writeStoreErr(w, err)
			return
		}
	}
	if body.Done != nil {
		task, err := store.SetDone(r.Context(), s.db, userID, id, *body.Done)
		if err != nil {
			writeStoreErr(w, err)
			return
		}
		writeJSON(w, http.StatusOK, task)
		return
	}
	tasks, err := store.ListTasks(r.Context(), s.db, userID)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "update failed"})
		return
	}
	for _, item := range tasks {
		if item.ID == id {
			writeJSON(w, http.StatusOK, item)
			return
		}
	}
	writeJSON(w, http.StatusNotFound, map[string]string{"error": "not found"})
}

func (s *Server) apiLogout(w http.ResponseWriter, r *http.Request) {
	if err := store.Logout(r.Context(), s.db, currentToken(r.Context())); err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "logout failed"})
		return
	}
	w.WriteHeader(http.StatusNoContent)
}

func (s *Server) apiDelete(w http.ResponseWriter, r *http.Request) {
	id, err := pathID(r)
	if err != nil {
		writeJSON(w, http.StatusNotFound, map[string]string{"error": "not found"})
		return
	}
	if err := store.DeleteTask(r.Context(), s.db, currentUser(r.Context()).ID, id); err != nil {
		writeStoreErr(w, err)
		return
	}
	w.WriteHeader(http.StatusNoContent)
}

func writeStoreErr(w http.ResponseWriter, err error) {
	if errors.Is(err, store.ErrNotFound) {
		writeJSON(w, http.StatusNotFound, map[string]string{"error": "not found"})
		return
	}
	if errors.Is(err, store.ErrInvalid) {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "invalid"})
		return
	}
	writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "request failed"})
}

func decodeJSON(w http.ResponseWriter, r *http.Request, dst any) error {
	r.Body = http.MaxBytesReader(w, r.Body, 1<<20)
	dec := json.NewDecoder(r.Body)
	dec.DisallowUnknownFields()
	if err := dec.Decode(dst); err != nil {
		return err
	}
	if err := dec.Decode(&struct{}{}); err != io.EOF {
		return errors.New("trailing json")
	}
	return nil
}

func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	if err := json.NewEncoder(w).Encode(v); err != nil {
		return
	}
}
```


### internal/web/server_test.go

```go
package web

import (
	"bytes"
	"context"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strconv"
	"strings"
	"testing"

	"example.com/taskboard/internal/store"
)

func TestPageAndAPIFlow(t *testing.T) {
	srv := newTestServer(t)
	h := srv.Handler()

	res := postForm(t, h, "/register", "email=ada@example.com&password=correct-horse", nil)
	if res.StatusCode != http.StatusSeeOther {
		body, _ := io.ReadAll(res.Body)
		t.Fatalf("register status %d body %s", res.StatusCode, body)
	}
	cookie := res.Cookies()
	if len(cookie) != 1 || cookie[0].Name != "session" || !cookie[0].HttpOnly {
		t.Fatalf("cookie: %+v", cookie)
	}

	list := httptest.NewRequest(http.MethodGet, "/tasks", nil)
	list.AddCookie(cookie[0])
	list.Host = "example.com"
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, list)
	if rec.Code != http.StatusOK || !strings.Contains(rec.Body.String(), "ada@example.com") {
		t.Fatalf("tasks page %d %s", rec.Code, rec.Body.String())
	}

	create := httptest.NewRequest(http.MethodPost, "/tasks", strings.NewReader("title=买牛奶"))
	create.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	create.Header.Set("HX-Request", "true")
	create.Header.Set("Origin", "http://example.com")
	create.Host = "example.com"
	create.AddCookie(cookie[0])
	rec = httptest.NewRecorder()
	h.ServeHTTP(rec, create)
	if rec.Code != http.StatusOK || !strings.Contains(rec.Body.String(), "买牛奶") {
		t.Fatalf("htmx create %d %s", rec.Code, rec.Body.String())
	}

	badOrigin := httptest.NewRequest(http.MethodPost, "/tasks", strings.NewReader("title=x"))
	badOrigin.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	badOrigin.Header.Set("Origin", "http://evil.example")
	badOrigin.Host = "example.com"
	badOrigin.AddCookie(cookie[0])
	rec = httptest.NewRecorder()
	h.ServeHTTP(rec, badOrigin)
	if rec.Code != http.StatusForbidden {
		t.Fatalf("origin status %d", rec.Code)
	}

	apiBody := bytes.NewBufferString(`{"email":"bea@example.com","password":"correct-horse"}`)
	apiReq := httptest.NewRequest(http.MethodPost, "/api/register", apiBody)
	apiReq.Header.Set("Origin", "http://localhost:5173")
	rec = httptest.NewRecorder()
	h.ServeHTTP(rec, apiReq)
	if rec.Code != http.StatusCreated {
		t.Fatalf("api register %d %s", rec.Code, rec.Body.String())
	}
	if got := rec.Header().Get("Access-Control-Allow-Origin"); got != "http://localhost:5173" {
		t.Fatalf("cors %q", got)
	}
	var tok struct {
		Token string `json:"token"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &tok); err != nil || tok.Token == "" {
		t.Fatal(err, rec.Body.String())
	}

	taskReq := httptest.NewRequest(http.MethodPost, "/api/tasks", strings.NewReader(`{"title":"写接口"}`))
	taskReq.Header.Set("Authorization", "Bearer "+tok.Token)
	rec = httptest.NewRecorder()
	h.ServeHTTP(rec, taskReq)
	if rec.Code != http.StatusCreated {
		t.Fatalf("api create %d %s", rec.Code, rec.Body.String())
	}

	var created store.Task
	if err := json.Unmarshal(rec.Body.Bytes(), &created); err != nil {
		t.Fatal(err)
	}
	patch := httptest.NewRequest(http.MethodPatch, "/api/tasks/"+itoa(created.ID), strings.NewReader(`{"done":true}`))
	patch.Header.Set("Authorization", "Bearer "+tok.Token)
	rec = httptest.NewRecorder()
	h.ServeHTTP(rec, patch)
	if rec.Code != http.StatusOK || !strings.Contains(rec.Body.String(), `"done":true`) {
		t.Fatalf("patch %d %s", rec.Code, rec.Body.String())
	}

	cross := httptest.NewRequest(http.MethodDelete, "/api/tasks/"+itoa(created.ID), nil)
	cross.Header.Set("Authorization", "Bearer "+cookie[0].Value)
	rec = httptest.NewRecorder()
	h.ServeHTTP(rec, cross)
	if rec.Code != http.StatusNotFound {
		t.Fatalf("cross delete %d %s", rec.Code, rec.Body.String())
	}
}

func postForm(t *testing.T, h http.Handler, path, body string, cookie *http.Cookie) *http.Response {
	t.Helper()
	req := httptest.NewRequest(http.MethodPost, path, strings.NewReader(body))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("Origin", "http://example.com")
	req.Host = "example.com"
	if cookie != nil {
		req.AddCookie(cookie)
	}
	rec := httptest.NewRecorder()
	h.ServeHTTP(rec, req)
	return rec.Result()
}

func newTestServer(t *testing.T) *Server {
	t.Helper()
	dsn := "file:" + filepath.Join(t.TempDir(), "app.db") + "?_pragma=foreign_keys(1)"
	db, err := store.Open(dsn)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { db.Close() })
	if err := store.Migrate(context.Background(), db); err != nil {
		t.Fatal(err)
	}
	log := slog.New(slog.NewTextHandler(io.Discard, nil))
	srv, err := New(db, log)
	if err != nil {
		t.Fatal(err)
	}
	return srv
}

func itoa(id int64) string {
	return strconv.FormatInt(id, 10)
}
```


### internal/web/templates/head.html

```html
{{define "head"}}<!DOCTYPE html>
<html lang="zh-Hans">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{{.Title}}</title>
  <script src="https://unpkg.com/htmx.org@2.0.4"></script>
  <style>
    :root { color-scheme: light; }
    body { margin: 0; font: 16px/1.5 "Segoe UI", "Noto Sans SC", sans-serif; background: #f6f5f2; color: #1c1b19; }
    main { max-width: 36rem; margin: 0 auto; padding: 2rem 1rem 4rem; }
    h1 { font-size: 1.5rem; font-weight: 650; margin: 0 0 1rem; }
    form.stack, .stack { display: flex; flex-direction: column; gap: 0.5rem; }
    label { font-size: 0.9rem; }
    input[type="email"], input[type="password"], input[type="text"] { font: inherit; padding: 0.45rem 0.55rem; border: 1px solid #c8c4bc; border-radius: 6px; background: #fff; }
    button { font: inherit; padding: 0.35rem 0.7rem; border: 1px solid #1c1b19; border-radius: 6px; background: #1c1b19; color: #fff; cursor: pointer; }
    button.ghost { background: #fff; color: #1c1b19; }
    .error { color: #8d1d18; min-height: 1.2em; margin: 0; }
    header.bar { display: flex; justify-content: space-between; align-items: center; margin-bottom: 1rem; }
    ul.tasks { list-style: none; padding: 0; margin: 1rem 0 0; display: flex; flex-direction: column; gap: 0.5rem; }
    li.task { display: flex; gap: 0.75rem; align-items: center; justify-content: space-between; background: #fff; border: 1px solid #e4e0d8; border-radius: 8px; padding: 0.55rem 0.7rem; }
    li.task .title.done { text-decoration: line-through; color: #6b6760; }
    .actions { display: flex; gap: 0.4rem; }
    .row { display: flex; gap: 0.5rem; }
    .row input[type="text"] { flex: 1; }
    a { color: inherit; }
  </style>
</head>
<body>
<main>
{{end}}

{{define "foot"}}
</main>
</body>
</html>
{{end}}
```


### internal/web/templates/auth.html

```html
{{define "login-page"}}{{template "head" .}}
<h1>登录</h1>
<form class="stack" action="/login" method="post">
  <label>邮箱 <input type="email" name="email" required autocomplete="username" value="{{.Email}}"></label>
  <label>密码 <input type="password" name="password" required autocomplete="current-password"></label>
  <button>登录</button>
</form>
<p class="error">{{.Error}}</p>
<p>没有账号？<a href="/register">注册</a></p>
{{template "foot" .}}{{end}}

{{define "register-page"}}{{template "head" .}}
<h1>注册</h1>
<form class="stack" action="/register" method="post">
  <label>邮箱 <input type="email" name="email" required autocomplete="username" value="{{.Email}}"></label>
  <label>密码 <input type="password" name="password" required minlength="8" autocomplete="new-password"></label>
  <button>注册</button>
</form>
<p class="error">{{.Error}}</p>
<p>已有账号？<a href="/login">登录</a></p>
{{template "foot" .}}{{end}}
```


### internal/web/templates/tasks.html

```html
{{define "task-item"}}
<li class="task" id="task-{{.ID}}">
  <span class="title{{if .Done}} done{{end}}">{{.Title}}</span>
  <span class="actions">
    <form action="/tasks/{{.ID}}/toggle" method="post" hx-post="/tasks/{{.ID}}/toggle" hx-target="closest li" hx-swap="outerHTML">
      <button class="ghost" type="submit">{{if .Done}}标为未完成{{else}}标为完成{{end}}</button>
    </form>
    <form action="/tasks/{{.ID}}/delete" method="post" hx-post="/tasks/{{.ID}}/delete" hx-target="closest li" hx-swap="delete">
      <button class="ghost" type="submit">删除</button>
    </form>
  </span>
</li>
{{end}}

{{define "tasks-page"}}{{template "head" .}}
<header class="bar">
  <span>{{.Email}}</span>
  <form action="/logout" method="post"><button class="ghost" type="submit">退出</button></form>
</header>
<h1>任务</h1>
<form class="row" action="/tasks" method="post" hx-post="/tasks" hx-target="#task-list" hx-swap="afterbegin" hx-on::after-request="if (event.detail.successful && event.detail.target && event.detail.target.id === 'task-list') this.reset()">
  <input type="text" name="title" maxlength="200" required placeholder="要做的事">
  <button type="submit">添加</button>
</form>
<p class="error" id="form-error">{{.Error}}</p>
<ul class="tasks" id="task-list">
  {{range .Tasks}}{{template "task-item" .}}{{end}}
</ul>
{{template "foot" .}}{{end}}
```


### frontend/package.json

```json
{
  "name": "taskboard-frontend",
  "private": true,
  "scripts": {
    "dev": "vite",
    "build": "vite build"
  },
  "devDependencies": {
    "vite": "^7.1.0"
  }
}
```


### frontend/index.html

```html
<!DOCTYPE html>
<html lang="zh-Hans">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>任务板</title>
    <link rel="stylesheet" href="/styles.css" />
  </head>
  <body>
    <main id="app"></main>
    <script type="module" src="/main.js"></script>
  </body>
</html>
```


### frontend/styles.css

```css
body {
  margin: 0;
  font: 16px/1.5 "Segoe UI", "Noto Sans SC", sans-serif;
  background: #f6f5f2;
  color: #1c1b19;
}

main {
  max-width: 36rem;
  margin: 0 auto;
  padding: 2rem 1rem 4rem;
}

form, label {
  display: flex;
  gap: 0.5rem;
}

label {
  flex-direction: column;
}

input, button {
  font: inherit;
  padding: 0.4rem 0.6rem;
}

.error {
  color: #8d1d18;
  min-height: 1.2em;
}

.done {
  text-decoration: line-through;
  color: #6b6760;
}

ul {
  list-style: none;
  padding: 0;
}

li, header {
  display: flex;
  gap: 0.6rem;
  align-items: center;
  justify-content: space-between;
}
```


### frontend/main.js

```javascript
const API = "http://localhost:8080";
const app = document.querySelector("#app");

const state = {
  token: sessionStorage.getItem("token") || "",
  email: sessionStorage.getItem("email") || "",
  tasks: [],
  error: "",
  mode: "login",
};

function escapeHTML(value) {
  return String(value).replace(/[&<>"']/g, (ch) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#39;",
  })[ch]);
}

async function request(path, options = {}) {
  const headers = new Headers(options.headers);
  if (options.body) {
    headers.set("Content-Type", "application/json");
  }
  if (state.token) {
    headers.set("Authorization", `Bearer ${state.token}`);
  }
  const response = await fetch(API + path, { ...options, headers });
  if (response.status === 204) {
    return null;
  }
  const data = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(data.error || response.statusText);
  }
  return data;
}

function render() {
  app.innerHTML = state.token ? tasksView() : authView();
  bind();
}

function authView() {
  const label = state.mode === "login" ? "登录" : "注册";
  const switchLabel = state.mode === "login" ? "去注册" : "去登录";
  return `
    <h1>${label}</h1>
    <form id="auth">
      <label>邮箱 <input name="email" type="email" required></label>
      <label>密码 <input name="password" type="password" minlength="8" required></label>
      <button>${label}</button>
    </form>
    <p class="error">${escapeHTML(state.error)}</p>
    <button id="switch" type="button">${switchLabel}</button>
  `;
}

function tasksView() {
  const items = state.tasks.map((task) => `
    <li>
      <span class="${task.done ? "done" : ""}">${escapeHTML(task.title)}</span>
      <button data-toggle="${task.id}">${task.done ? "标为未完成" : "标为完成"}</button>
      <button data-delete="${task.id}">删除</button>
    </li>
  `).join("");
  return `
    <header>
      <span>${escapeHTML(state.email)}</span>
      <button id="logout" type="button">退出</button>
    </header>
    <h1>任务</h1>
    <form id="create">
      <input name="title" maxlength="200" required placeholder="要做的事">
      <button>添加</button>
    </form>
    <p class="error">${escapeHTML(state.error)}</p>
    <ul>${items}</ul>
  `;
}

function bind() {
  const auth = document.querySelector("#auth");
  if (auth) {
    auth.addEventListener("submit", async (event) => {
      event.preventDefault();
      const form = new FormData(auth);
      try {
        const data = await request(`/api/${state.mode}`, {
          method: "POST",
          body: JSON.stringify({
            email: form.get("email"),
            password: form.get("password"),
          }),
        });
        state.token = data.token;
        state.email = String(form.get("email"));
        sessionStorage.setItem("token", state.token);
        sessionStorage.setItem("email", state.email);
        state.error = "";
        await loadTasks();
      } catch (err) {
        state.error = err.message;
        render();
      }
    });
  }
  const switcher = document.querySelector("#switch");
  if (switcher) {
    switcher.addEventListener("click", () => {
      state.mode = state.mode === "login" ? "register" : "login";
      state.error = "";
      render();
    });
  }
  const create = document.querySelector("#create");
  if (create) {
    create.addEventListener("submit", async (event) => {
      event.preventDefault();
      const title = new FormData(create).get("title");
      try {
        await request("/api/tasks", {
          method: "POST",
          body: JSON.stringify({ title }),
        });
        state.error = "";
        await loadTasks();
      } catch (err) {
        state.error = err.message;
        render();
      }
    });
  }
  document.querySelectorAll("[data-toggle]").forEach((button) => {
    button.addEventListener("click", async () => {
      const task = state.tasks.find((item) => String(item.id) === button.dataset.toggle);
      await request(`/api/tasks/${button.dataset.toggle}`, {
        method: "PATCH",
        body: JSON.stringify({ done: !task.done }),
      });
      await loadTasks();
    });
  });
  document.querySelectorAll("[data-delete]").forEach((button) => {
    button.addEventListener("click", async () => {
      await request(`/api/tasks/${button.dataset.delete}`, { method: "DELETE" });
      await loadTasks();
    });
  });
  const logout = document.querySelector("#logout");
  if (logout) {
    logout.addEventListener("click", async () => {
      try {
        await request("/api/logout", { method: "POST" });
      } catch (err) {
        state.error = err.message;
      }
      state.token = "";
      state.email = "";
      state.tasks = [];
      sessionStorage.clear();
      render();
    });
  }
}

async function loadTasks() {
  try {
    state.tasks = await request("/api/tasks");
    state.error = "";
  } catch (err) {
    state.error = err.message;
    if (String(err.message).includes("login")) {
      state.token = "";
      sessionStorage.clear();
    }
  }
  render();
}

if (state.token) {
  loadTasks();
} else {
  render();
}
```


### Dockerfile

```dockerfile
FROM golang:1.27 AS build
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .
ENV CGO_ENABLED=0
RUN go build -o /out/taskboard ./cmd/taskboard

FROM gcr.io/distroless/static-debian12
COPY --from=build /out/taskboard /taskboard
ENV TASKBOARD_ADDR=:8080
EXPOSE 8080
ENTRYPOINT ["/taskboard"]
```


### 25.1 你现在可以改的三件事

1. 给任务加一个截止时间。从 `Task` 结构体、表、校验、模板和 JavaScript 一起改。漏掉任何一层，测试或页面都会告诉你。
2. 把登录失败的 API 判断从错误字符串改成状态码。
3. 按第 18 章的清单把 SQLite 换成 PostgreSQL。HTTP 测试可以先继续用 SQLite，存储测试再指向 PostgreSQL。

做完这三件，这条前后端路线就从「跟着读」变成「能改」。第 14 章里的 gRPC、控制器和消息队列仍然是后面的地图，不在这个程序里。

---

## 附录　练习答案

**第 0 章**

1. 大约 200 个服务连接的 goroutine，外加少量日志或清理用的 goroutine。操作系统线程大约是 CPU 核数那一量级，由调度器把 goroutine 放上去跑。
2. 返回零值和一个非 nil 的 `error`。调用方决定状态码或继续包装，不使用那个零值结果。
3. 读任务的函数返回 `Task` 和 `error`。页面处理函数把它写成 HTML，接口处理函数把它写成 JSON。

**第 1 章**

1. 含有 `go1.27.3` 的一行版本文字。
2. `go run` 运行结束后不在项目目录留下可执行文件。`go build -o 名字` 会留下。两者都会编译。
3. `gofmt` 会统一缩进、运算符两边的空格和导入分组。它不改函数含义。

**第 2 章**

1. 小写开头的名字只在本包可见。
2. 避免「导入了但没有使用」的编译错误，同时仍执行该包的 `init`，让驱动注册自己。
3. 放在 `main`。读配置会失败，失败时要有日志和退出码。`init` 适合不会失败的包内准备。

**第 3 章**

1. `var n int` 可以出现在函数外，零值是 0。`:=` 只能在函数内，并且左值里至少有一个新名字。
2. `len` 数的是字节。一个汉字占 3 个字节，200 字的中文标题会被提前拒绝。应按 rune 计数。
3. `Warn` 的数值会从 2 变成 3。已经写进数据库的旧数字会指向新的含义。所以持久化状态不用 iota 的隐式顺序。

**第 4 章**

1. `b` 和 `a` 共享背后的数组，容量还够，`append` 就地写下标 2。`a[:2:2]` 把容量限制成 2，再追加就会分配。
2. `nil` 切片编码成 `null`，空切片编码成 `[]`。客户端处理列表时只面对一种空结果。
3. `nil` 表示没有填写，指向 0 的指针表示填写了 0。普通 `int` 的 0 两种情况都是。

**第 5 章**

1. `tasks[i].Done = true`。
2. 参数把「这个 goroutine 看见的值」写在调用点上。读代码的人不用回忆循环变量在哪个版本里如何捕获。
3. 不会。它直接进入下一个 case 的函数体。

**第 6 章**

1. 命名结果在 `return` 表达式赋值之后、函数真正离开之前还能被 defer 修改。
2. 参数在执行到 `defer` 那一行时求值。后来给变量 `f` 赋新值，推迟调用仍关上原来的那个文件。
3. defer 要等到外层函数返回才运行。循环中的文件会一直占着描述符。应让每一次打开在一个小函数里用 defer 关闭。

**第 7 章**

1. 接口值同时保存类型 `*bytes.Buffer` 和 `nil` 指针。类型信息还在，所以接口不等于 `nil`。
2. 只有 `*Task`。值接收者方法对值和指针都在方法集里，指针接收者方法只在指针的方法集里。
3. 小接口。假实现只要满足眼前这个行为，不必把 15 个方法都写出来。

**第 8 章**

1. `%v` 只留下文字，`errors.Is` 认不出原来的哨兵。`%w` 保留原错误。
2. 返回 `ErrConflict`。这是可预期的业务结果。
3. 装了 `nil` 指针的接口不等于 `nil`。调用方会以为有值，调用时 panic。失败应返回接口的 `nil`。

**第 9 章**

1. `any` 不能比较。`comparable` 才保证 `==` 合法。
2. 不能。接口方法不能声明类型参数，也不能用泛型方法去实现。
3. 不要。它只创建 `Task`。泛型不会减少这里的任何重复。

**第 10 章**

1. 发送方一直阻塞。
2. 发送方关闭。接收方关闭，或关闭两次，会 panic。
3. 若 `Add` 在 goroutine 内部，`Wait` 可能在 `Add` 发生前就看到计数为 0 并返回。

**第 11 章**

1. v2 及以上的主版本写在导入路径里。v0 和 v1 不写。`x/crypto` 仍是 v0。
2. 它保存依赖的校验和。下载内容与记录不符时，构建停止。
3. 同事的磁盘上没有那条路径，构建会失败。本地临时替换不要留在给别人用的提交里。

**第 12 章**

1. `Fatal` 立刻结束当前测试函数。`Error` 记录失败并继续执行。
2. 并行测试会同时写同一个文件，结果互相覆盖，SQLite 也会报锁。各自使用 `t.TempDir()`。
3. 例如「标题刚好 200 个汉字可以通过，201 个不行」这种边界。语句都跑到过，不代表边界值被选中过。

**第 13 章**

1. UTC 不随运行机器的本地时区变化。RFC3339 可以无歧义地解析。显示时再换到 `Asia/Hong_Kong`。
2. `//go:embed` 必须紧挨它约束的变量。中间插入其他语句，这行就不再是对该变量的指令，编译会失败或文件不会被嵌入。
3. v2 默认拒绝重复名字。v1 API 为了兼容仍按原来的方式接受，后出现的值覆盖先出现的值。

**第 14 章**

1. `context.Context`。
2. 用户不用先安装语言运行时。一个文件加上它要读的数据就够了。
3. 消息可能投递多次。第二次执行应当得到和第一次相同的业务结果，而不是再插入一份。

**第 15 章**

1. 303 让浏览器跳到列表。刷新时浏览器请求的是 GET 列表，不会再次提交表单。
2. 404 不透露这个编号是否存在于别人的账号下。
3. 它限制客户端发送请求头的时间，避免连接被极慢的头部长期占住。

**第 16 章**

1. 页面可以把浏览器送到登录表单。API 的调用方是脚本，需要明确的 401。
2. `Shutdown` 停止接新连接，并给已有请求一段收尾时间。直接退出会立刻断开这些响应。
3. `Authorization` 和 `Cookie`。

**第 17 章**

1. `bool` 的缺省和 `false` 都是 false。指针为 `nil` 表示请求没带这个字段。
2. 防客户端把非常大的正文慢慢发给解码器，占满内存。
3. `err.Error()` 可能含有 SQL、路径或驱动细节。这些写进日志。客户端只需要知道这次失败了。

**第 18 章**

1. 比较放在 SQL 里，一次查询就完成「找不到或不属于你」。少一次忘记比较的机会。
2. 池里每个连接有自己的内存库，建表的那个连接和查询的那个连接可能不是同一个。
3. 前 72 个字节。更长的部分不参与哈希。

**第 19 章**

1. `HttpOnly` 让页面脚本读不到会话 Cookie。`SameSite=Lax` 让其他网站发起的跨站 POST 不附带这个 Cookie。
2. Cookie 只在这一个浏览器上消失。令牌若被复制出去，服务端会话仍有效。删除会话行才使令牌失效。
3. 用户可以改隐藏字段。身份来自服务端保存的会话。

**第 20 章**

1. Cookie 写请求会检查 Origin 主机是否等于 Host。测试要走通成功路径，就必须让它们相同。另一则测试故意让它们不同。
2. 模板在编译期打进可执行文件。运行时按嵌入的文件系统读取，不查找当前目录。
3. 编译在第一阶段已经完成。运行只需要那个静态链接的可执行文件。

**第 21 章**

1. 文本上下文里会写成 `&lt;script&gt;alert(1)&lt;/script&gt;`，浏览器显示这段文字。
2. 点变成切片中的当前元素，在任务列表里就是一个 `Task`。
3. 密码是秘密。失败后的页面可能被旁人看到，输入框不应保留它。校验失败也不说明原密码是对的。

**第 22 章**

1. 会。表单有 `action` 和 `method`。服务器返回 303，浏览器打开任务列表。
2. 它让错误句子进入 `#form-error`，而不是被插进任务列表当成一条任务。
3. `hx-swap="delete"` 只把目标元素从文档里去掉，不解析响应正文。

**第 23 章**

1. 401。CORS 只允许脚本阅读响应，不代替登录。
2. 预检的路径和方法组合很多。放在路由外，一次处理所有 `OPTIONS`，不必给每条 API 复制一条预检路由。
3. 查询字符串会进访问日志、浏览器历史和别人的 Referer。令牌放在 `Authorization` 头里。

**第 24 章**

1. `innerHTML` 不会像 `html/template` 那样按上下文转义。标题必须先变成转义后的文字。
2. 204 没有正文。`response.json()` 会因为空正文失败。
3. 要多返回状态码，或在错误对象上带上 `status`。判断 401 时就不用搜索错误句子。

---

教程到这里结束。语言部分从第 2 章到第 10 章，Web 部分从第 15 章到第 25 章。第 14 章留下的方向，等任务板你能独立改动之后再单开。
