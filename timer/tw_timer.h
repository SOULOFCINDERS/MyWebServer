#ifndef TIME_WHEEL_TIMER
#define TIME_WHEEL_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

// 时间轮调度器
// 时间轮：每个槽的时间间隔相同，每个槽中的定时器数量不同

class tw_timer;
#define BUFFER_SIZE 64

struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    tw_timer *timer;
};

class tw_timer
{
public:
    tw_timer(int rot, int ts) : next(NULL), prev(NULL), rotation(rot), time_slot(ts) {}

public:
    int rotation;                   // 记录定时器在时间轮转多少圈后生效
    int time_slot;                  // 记录定时器属于时间轮上哪个槽（对应的链表，下同）
    void (*cb_func)(client_data *); // 定时器回调函数
    client_data *user_data;         // 客户数据

    tw_timer *next;
    tw_timer *prev;
};

class time_wheel
{
public:
    time_wheel() : cur_slot(0){};  // 初始化当前槽为0
    ~time_wheel();
    tw_timer *add_timer(int timeout); // 添加定时器，定时器的超时时间为timeout
    void del_timer(tw_timer *timer); // 删除目标定时器timer
    void tick(); // 时间轮向前滚动一次，即它的槽的数目增加1

private:
    static const int N = 60; // 时间轮上槽的数目
    static const int SI = 1; // 每1s时间轮转动一次，即槽间隔为1s
    tw_timer *slots[N];      // 时间轮的槽，其中每个元素指向一个定时器链表，链表无序
    int cur_slot;            // 时间轮的当前槽
};
class Utils // 工具类
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    // 对文件描述符设置非阻塞
    int setnonblocking(int fd);

    // 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    // 信号处理函数
    static void sig_handler(int sig);

    // 设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    // 定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    time_wheel m_time_wheel;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif