## Reference

Program:

* [yorickdewid/Chat-Server](https://github.com/yorickdewid/Chat-Server)
* [amonmoce/ChatRoom](https://github.com/amonmoce/ChatRoom)
* [Simple Telnet Client](https://gist.github.com/legnaleurc/7638738)
* [Chatroom in C / Socket programming in Linux](https://stackoverflow.com/q/19349084/3737970)

Makefile:

* [Make: how to continue after a command fails?](https://stackoverflow.com/q/2670130/3737970)
* [How to use LDFLAGS in makefile](https://stackoverflow.com/q/13249610/3737970)
* [Building And Using Static And Shared "C" Libraries](http://docencia.ac.upc.edu/FIB/USO/Bibliografia/unix-c-libraries.html)
* [Practical Makefiles, by example](http://nuclear.mutantstargoat.com/articles/make/)

TCP:

* [Message Framing](https://blog.stephencleary.com/2009/04/message-framing.html)
* [What is a message boundary?](https://stackoverflow.com/q/9563563/3737970)
* [TCP Message Framing](https://blog.chrisd.info/tcp-message-framing/)
* [Five pitfalls of Linux sockets programming](https://developer.ibm.com/tutorials/l-sockpit/)
* [Understanding the Internet: How Messages Flow Through TCP Sockets](https://andrewskotzko.com/understanding-the-internet-how-messages-flow-through-tcp-sockets/)

## Solution

**问题 1：** 如何实现客户端既能从服务端读取消息，也能接受终端输入发往服务器的消息？

**方案 1：**

把 `STDIN_FILENO` [^1] 也加入 epoll 监控中去。

这样如果终端有输入的话，按下 <kdb>Enter</kdb> 后，`STDIN_FILENO` 就会准备好。解除 `epoll_wait()` 的阻塞，进入向服务器写入分支；如果有消息从服务器过来，同样 `epoll_wait()` 也会解除阻塞，进入从服务器读取分支。

参考：[amonmoce/ChatRoom](https://github.com/amonmoce/ChatRoom)

[^1]: [What is the difference between stdin and STDIN_FILENO?](https://stackoverflow.com/q/15102992/3737970)

**方案 2：**

在客户端见一个消息列表，在需要的读的时候读取这个列表。也可以把已读消息放到消息历史列表中，而不销毁消息。

----

**问题 2：** 如何实现心跳机制？

**方案 1：**

服务端使用多线程，线程阻塞在 `read()`。客户端使用 epoll，设置超时后，主动向服务端发送一个消息，表明自己活着。当服务端读取失败后（`read() <= 0`），从在线列表中删除此客户端。

**方案 2：**

服务端采用 `select` 或者 `epoll`，在客户端结构体（`client_t`）加入一个字段（如 `is_live`），在用户接入时初始化为某个值（如 `9`）。开辟一个线程，让 `is_live` 字段每隔 1 秒减 1；而每当客户端有输入传入重新赋值 `is_live` 的值为 `9`。

客户端需要设置超时主动发送一个数据给服务端，这样表明自己还活着。

----

**问题 3：** 如何检查 mutex 是否被当前进程加锁了？

**方案：**

```c
#include <pthread.h>
#include <errno.h>

if (EBUSY == pthread_mutex_trylock(&clients_mutex))
    pthread_mutex_unlock(&clients_mutex);
```

----

**问题 4：** 用 C 修改文件存在的内容

**方案：**

使用 `lseek()` 函数。[^2]

```c
#include <stdio.h>
#include <stdlib.h>

int main()
{
    FILE *ft;
    char const *name = "abc.txt";
    int ch;
    ft = fopen(name, "r+");
    if (ft == NULL)
    {
        fprintf(stderr, "cannot open target file %s\n", name);
        exit(1);
    }
    while ((ch = fgetc(ft)) != EOF)
    {
        if (ch == 'i')
        {
            fseek(ft, -1, SEEK_CUR);
            fputc('a',ft);
            fseek(ft, 0, SEEK_CUR);
        }
    }
    fclose(ft);
    return 0;
}
```

>>>
**Input followed by output requires seeks**

The `fseek(ft, 0, SEEK_CUR);` statement is required by the C standard.
>>>

**不要用于以文本形式打开的文件修改。** 如果写入文件字符串过长，会覆盖掉后面的内容。

写入一个结构体，然后修改结构体，再写入。这样可以。

[^2]: [modify existing contents of file in c](https://stackoverflow.com/q/21958155/3737970)
