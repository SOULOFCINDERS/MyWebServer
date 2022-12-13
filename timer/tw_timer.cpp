#include "tw_timer.h"
#include "../http/http_conn.h"

time_wheel::time_wheel()
{
    for (int i = 0; i < N; ++i)
    {
        slots[i] = NULL;
    }
}

time_wheel::~time_wheel()
{
    for (int i = 0; i < N; ++i)
    {
        tw_timer *tmp = slots[i]; // 取得每个槽的头结点
        while (tmp)               // 遍历每个槽，并销毁其中的定时器
        {
            slots[i] = tmp->next; // 将当前槽的头结点指向下一个节点
            delete tmp;           // 销毁当前节点
            tmp = slots[i];       // 将tmp指向下一个节点
        }
    }
}

tw_timer *time_wheel::add_timer(int timeout) 
{
    if (timeout < 0)
    {
        return NULL;
    }
    int ticks = 0;
    if (timeout < SI) //
    {
        ticks = 1;
    }
    else
    {
        ticks = timeout / SI;
    }
    int rotation = ticks / N;                     // 计算定时器在时间轮转动多少圈后生效
    int ts = (cur_slot + (ticks % N)) % N;        // 计算定时器应该被插入哪个槽中
    tw_timer *timer = new tw_timer(rotation, ts); // 创建新的定时器，它在时间轮转动rotation圈后生效，且位于第ts个槽上

    if (!slots[ts]) // 如果第ts个槽中尚无任何定时器，则把新建的定时器插入其中，并将该定时器设置为该槽的头结点
    {
        printf("add timer, rotation is %d, ts is %d, cur_slot is %d", rotation, ts, cur_slot);
        slots[ts] = timer;
    }
    else // 否则，将定时器插入第ts个槽中
    {
        timer->next = slots[ts]; // 将定时器插入第ts个槽的链表头部
        slots[ts]->prev = timer; // 将第ts个槽的原头结点的前向指针指向新的头结点
        slots[ts] = timer; // 将新的头结点设置为第ts个槽的头结点
    }
    return timer;

}

void time_wheel::del_timer(tw_timer *timer)
{
    if(!timer)
    {
        return;
    }
    int ts = timer->time_slot; // 记录目标定时器所在的槽
    if(timer==slots[ts]) // 如果目标定时器就是槽的头结点，则需要重置第ts个槽的头结点
    {
        slots[ts]=slots[ts]->next;
        if(slots[ts]) // 如果新的头结点不为空，则将新的头结点的前向指针置空
        {
            slots[ts]->prev=NULL;
        }
        delete timer;
    }
    else // 如果目标定时器不是槽的头结点，则把它从链表中删除
    {
        timer->prev->next=timer->next; //
        if(timer->next)
        {
            timer->next->prev=timer->prev;
        }
        delete timer;
    }
}

void time_wheel::tick() // 时间轮向前滚动一格，即把槽的下标向前移动一个单位
{
    tw_timer *tmp = slots[cur_slot]; // 取得时间轮上当前槽的头结点
    printf("current slot is %d\n", cur_slot);
    while (tmp) // 遍历当前槽中的每个定时器
    {
        printf("tick the timer once\n");
        if (tmp->rotation > 0) // 如果定时器的rotation值大于0，则它在这一轮不起作用
        {
            tmp->rotation--;
            tmp = tmp->next;
        }
        else // 否则，说明定时器已经到期，于是执行定时任务，然后删除该定时器
        {
            tmp->cb_func(tmp->user_data); // 执行定时任务
            if (tmp == slots[cur_slot])   // 如果被执行的定时器是时间轮上当前槽的头结点，则重置第cur_slot个槽的头结点
            {
                printf("delete header in cur_slot\n");
                slots[cur_slot] = tmp->next;
                delete tmp;
                if (slots[cur_slot])
                {
                    slots[cur_slot]->prev = NULL;
                }
                tmp = slots[cur_slot];
            }
            else // 如果被执行的定时器不是时间轮上当前槽的头结点，则将它删除
            {
                tmp->prev->next = tmp->next;
                if (tmp->next)
                {
                    tmp->next->prev = tmp->prev;
                }
                tw_timer *tmp2 = tmp->next;
                delete tmp;
                tmp = tmp2;
            }
        }
    }
    cur_slot = ++cur_slot % N; // 更新时间轮的当前槽，以反映时间轮的转动
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_time_wheel.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}




