//
// Created by molin on 2021/5/6.
//

#ifndef ABSMSERVER__WEBSERVER_H_
#define ABSMSERVER__WEBSERVER_H_

#include<sys/socket.h>
#include<netinet/in.h>
#include<stdio.h>
#include<unistd.h>
#include<errno.h>
#include<fcntl.h>
#include<stdlib.h>
#include<cassert>
#include<sys/epoll.h>
#include<string>

#include"./threadpool/threadpool.h"
#include"./http/http_conn.h"

const int MAX_FD = 65536;
const int MAX_EVENT_NUMBER = 10000;
const int TIMESLOT = 5;
using namespace std;

class WebServer {
 public:
  WebServer();
  ~WebServer();

  void init();
  void thread_pool();
  void sql_pool();
  void log_write();
  void trig_mode();
  void eventListen();
  void eventLoop();
  void timer();
  void adjust_timer();
  void deal_timer();
  bool dealwithsignal();
  void dealwithread();
  void dealwithwrite();

  // 基础
  int m_port;
  char *m_root;
  int m_log_write;
  int m_close_log;
  int m_actormodel;

  int m_pipefd[2];
  int m_epollfd;
  // http_conn *users;

  // 数据库相关
  // connection_pool *m_connPool;
  string m_user;
  string m_passWord;
  string m_databaseName;
  int m_sql_num;

  // 线程池
  // threadpool<http_conn> *m_pool;
  int m_thread_num;

  // epoll_event
  epoll_event events[MAX_EVENT_NUMBER];

  int m_listendf;
  int m_OPT_LINGER;
  int m_TRIGMode;
  int m_LISTENTrigmode;
  int m_CONNTrigmode;

  // 定时器相关
  // client_data *users_timer;
  // Utils utils;
};

#endif //ABSMSERVER__WEBSERVER_H_
