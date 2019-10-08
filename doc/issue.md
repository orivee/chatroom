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
* [Add .so and .a libraries to Makefile](https://stackoverflow.com/q/12054858/3737970)
* [Run make in each subdirectory](https://stackoverflow.com/q/17834582/3737970)
* [Makefiles and subdirectories](https://www.linuxquestions.org/questions/programming-9/makefiles-and-subdirectories-794088/)

TCP:

* [Message Framing](https://blog.stephencleary.com/2009/04/message-framing.html)
* [What is a message boundary?](https://stackoverflow.com/q/9563563/3737970)
* [TCP Message Framing](https://blog.chrisd.info/tcp-message-framing/)
* [Five pitfalls of Linux sockets programming](https://developer.ibm.com/tutorials/l-sockpit/)
* [Understanding the Internet: How Messages Flow Through TCP Sockets](https://andrewskotzko.com/understanding-the-internet-how-messages-flow-through-tcp-sockets/)

Variable arguments:

* `va_list`, `va_arg`, `va_end`
* `vsnprintf`, `vsprintf`
* [Variable Argument Lists in C using va_list](https://www.cprogramming.com/tutorial/c/lesson17.html)
* [How do varargs work in C?](https://jameshfisher.com/2016/11/23/c-varargs/)

Enum:

* [Print text instead of value from C enum](https://stackoverflow.com/q/3168306/3737970)
* [How to define an enumerated type (enum) in C?](https://stackoverflow.com/q/1102542/3737970)

Project Structure:

* [how to structure a multi-file c program: part 1](https://opensource.com/article/19/7/structure-multi-file-c-part-1)
* [how to structure your project · modern cmake](https://cliutils.gitlab.io/modern-cmake/chapters/basics/structure.html)
* [structuring a c project](https://splone.com/blog/2016/1/4/structuring*a*c*project/)
* [canonical project structure](http://open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1204r0.html#abstract)
* [jnyjny/meowmeow: meowmeow - a toy file encoder/decoder](https://github.com/jnyjny/meowmeow)
* [kostrahb/generic-c-project: c project structure i use with makefiles](https://github.com/kostrahb/generic-c-project)
* [is there a standard folder structure for programming projects? : c\_programming](https://www.reddit.com/r/c_programming/comments/3izgic/is_there_a_standard_folder_structure_for/)
* [folder structure for a c project - software engineering stack exchange](https://softwareengineering.stackexchange.com/questions/379202/folder-structure-for-a-c-project)
* [cs.swarthmore.edu/\~newhall/unixhelp/c\_codestyle.html](https://www.cs.swarthmore.edu/~newhall/unixhelp/c_codestyle.html)


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

----

**问题 5：** 如何解析命令行参数？

**方案：**

* [Parsing command-line arguments in C?](https://stackoverflow.com/q/9642732/3737970)
* [Argument-parsing helpers for C/Unix](https://stackoverflow.com/q/189972/3737970)
* [GUN Getopt](https://www.gnu.org/software/libc/manual/html_node/Getopt.html)

----

**问题 6：** 实现日志记录器

**方案：**

当前时间的获取：

```c
#include <time.h>

char * timenow()
{
    static char buffer[64];
    time_t rawtime;
    struct tm * timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, 64, "%Y-%m-%d %H:%M:%S", timeinfo);

    return buffer;
}
```

`fflush()` - flush a stream

* [rxi/log.c](https://github.com/rxi/log.c)
* [a simplified logging system using macros](https://coderwall.com/p/v6u7jq/a-simplified-logging-system-using-macros)
    * [dmcrodrigues/macro-logger](https://github.com/dmcrodrigues/macro-logger)
* [ntpeters/SimpleLogger](https://github.com/ntpeters/SimpleLogger)
* [storing logs/error message on C programming](https://stackoverflow.com/q/16116143/3737970)
