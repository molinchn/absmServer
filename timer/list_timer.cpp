//
// Created by molin on 2021/5/11.
//

#include "list_timer.h"
#include"../http/http_conn.h"

sort_timer_lst::sort_timer_lst() {
  head = NULL;
  tail = NULL;
}

sort_timer_lst::~sort_timer_lst() noexcept {
  util_timer *tmp = head;
  while (head) {
    head = tmp->next;
    delete tmp;
    tmp = head;
  }
}

void sort_timer_lst::add_timer(util_timer *timer) {
  if (!timer) return;
  if (!head) return;
  // 头插
  if (timer->expire < head->expire) {
    timer->next = head;
    head->prev = timer;
    head = timer;
    return;
  }
  // 非头插
  add_timer(timer, head);
}

// 重载：timer list的add函数，可以传入头节点
// 感觉有优化空间
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head) {
  util_timer *prev = lst_head;
  util_timer *tmp = prev->next;
  while (tmp && timer->expire >= tmp->expire) {
    prev = tmp;
    tmp = tmp->next;
  }
  if (tmp) {
    prev->next = timer;
    timer->next = tmp;
    tmp->prev = timer;
    timer->prev = prev;
  } else {
    prev->next = timer;
    timer->next = NULL;
    timer->prev = prev;
    tail = timer;
  }
}

void sort_timer_lst::adjust_timer(util_timer *timer) {
  if (!timer) {
    return;
  }
  util_timer *tmp = timer->next;
  // 这里如果在尾部则不处理
  // 如果不在尾部，但是超时值仍然小于下一个定时器，也不处理
  // 对这里的疑问注解：如果timer变小，不用往前移动？tick()能否及时处理变小的timer？
  // 这里是因为timer不会变小，重新设置只会往后延长超时时间，因此不用担心往前移动的问题。
  if (!tmp || timer->expire < tmp->expire) {
    return;
  }

  if (timer == head) {
    head = head->next;
    head->prev = NULL;
    timer->next = NULL;
    add_timer(timer, head);
  } else {
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    add_timer(timer, timer->next);
  }
}

// 删除timer
void sort_timer_lst::del_timer(util_timer *timer) {
  if (!timer) return;
  if (timer == head && timer == tail) {
    delete timer;
    head = NULL;
    tail = NULL;
    return;
  }
  if (timer == head) {
    head = head->next;
    head->prev = NULL;
    delete timer;
    return;
  }
  if (timer == tail) {
    tail = tail->prev;
    tail->next = NULL;
    delete timer;
    return;
  }
  timer->prev->next = timer->next;
  timer->next->prev = timer->prev;
  delete timer;
}

// tick()函数会把定时器链表中过期的链表执行其回调函数，并从链表中删除
void sort_timer_lst::tick() {
  if (!head) return;
  time_t cur = time(NULL);
  util_timer *tmp = head;
  while (tmp) {
    if (cur < tmp->expire) {
      break;
    }
    tmp->cb_func(tmp->user_data);

    head = tmp->next;
    if (head) head->prev = NULL;
    delete tmp;

    tmp = head;
  }
}

void Utils::init(int timeslot) {
  m_TIMESLOT = timeslot;
}

int Utils::setnonblocking(int fd) {
  // F_GETFL是获得fd的标志位
  int old_option = fcntl(fd, F_GETFL);
  // 然后把对应的标志位置为1
  int new_option = old_option | O_NONBLOCK;
  // F_SETFL是设置fd的标志位，成功则返回0
  fcntl(fd, F_SETFL, new_option);
  return old_option;
}

void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
  epoll_event event;
  event.data.fd = fd;

  // 数据可读和TCP被对方关闭两个事件一定会注册
  event.events = EPOLLIN | EPOLLRDHUP;
  // 根据标志位判定是否是ET模式，是否oneshot
  if (TRIGMode) event.events |= EPOLLET;
  if (one_shot) event.events |= EPOLLONESHOT;

  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
  setnonblocking(fd);
}

// 信号处理函数
void Utils::sig_handler(int sig) {
  // 保存errno，保证可重入性
  int save_errno = errno;
  int msg = sig;
  // 利用管道传递sig
  send(u_pipefd[1], (char *) &msg, 1, 0);
  errno = save_errno;
}

void Utils::addsig(int sig, void (*handler)(int), bool restart) {
  struct sigaction sa;
  memset(&sa, '\0', sizeof(sa));

  // sa.sa_handler是一个函数指针，指向的是信号处理函数
  sa.sa_handler = handler;
  // sa.sa_flags指定信号处理的行为，SA_RESTART表示被信号打断的系统调用自动重新发起
  if (restart) sa.sa_flags |= SA_RESTART;
  // sa.sa_mask指定信号处理函数在执行期间需要被屏蔽的信号（sigfillset将屏蔽所有信号）
  sigfillset(&sa.sa_mask);

  // 下面是对设定的信号处理模式进行应用
  assert(sigaction(sig, &sa, NULL) != -1);
}

void Utils::timer_handler() {
  // 检查链表
  m_timer_lst.tick();
  // 设置SIGALRM信号，在m_TIMESLOT秒后发送给当前的进程
  // 1. 如果设置了信号处理函数，则执行之
  // 2. 如果没有设置信号处理函数，则默认终止该进程
  alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info) {
  send(connfd, info, strlen(info), 0);
  close(connfd);
}

// 这两个是静态成员变量
int *Utils::u_pipefd = NULL;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data *user_data) {
  // 删除这个epoll事件
  epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
  assert(user_data);
  // 关闭这个socket
  close(user_data->sockfd);
  // 减少一个http连接
  http_conn::m_user_count--;
}