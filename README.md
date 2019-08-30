## Reference

Program:

* [yorickdewid/Chat-Server](https://github.com/yorickdewid/Chat-Server)
* [amonmoce/ChatRoom](https://github.com/amonmoce/ChatRoom)
* [Simple Telnet Client](https://gist.github.com/legnaleurc/7638738)
* [Chatroom in C / Socket programming in Linux](https://stackoverflow.com/q/19349084/3737970)

Makefile:

* [Make: how to continue after a command fails?](https://stackoverflow.com/q/2670130/3737970)

## Solution

**问题 1：** 如何实现客户端既能从服务端读取消息，也能接受终端输入发往服务器的消息？

**方案：**

把 `STDIN_FILENO` [^1] 也加入 epoll 监控中去。

这样如果终端有输入的话，按下 <kdb>Enter</kdb> 后，`STDIN_FILENO` 就会准备好。解除 `epoll_wait()` 的阻塞，进入向服务器写入分支；如果有消息从服务器过来，同样 `epoll_wait()` 也会解除阻塞，进入从服务器读取分支。

参考：[amonmoce/ChatRoom](https://github.com/amonmoce/ChatRoom)

[^1]: [What is the difference between stdin and STDIN_FILENO?](https://stackoverflow.com/q/15102992/3737970)
