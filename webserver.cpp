//
// Created by molin on 2021/5/6.
//

#include"webserver.h"

WebServer::WebServer() {
  users = new http_conn[MAX_FD];

  // char server_path[200];
  // getcwd(server_path, 200);
  // 由于使用clion调试，这里手动设置路径
  char server_path[] = "/home/molinchn/remoteProjects/absmServer";
  char root[6] = "/root";
  m_root = (char *) malloc(strlen(server_path) + strlen(root) + 1);
  strcpy(m_root, server_path);
  strcat(m_root, root);
  cout << "m_root = " << m_root << endl;

  users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer() {
  close(m_epollfd);
  close(m_listenfd);
  close(m_pipefd[1]);
  close(m_pipefd[0]);
  delete[] users;
  delete[] users_timer;
  delete m_pool;
}

void WebServer::init(int port,
                     string user,
                     string passWord,
                     string databaseName,
                     int log_write,
                     int opt_linger,
                     int trigmode,
                     int sql_num,
                     int thread_num,
                     int close_log,
                     int actor_model) {
  m_port = port;
  m_user = user;
  m_passWord = passWord;
  m_databaseName = databaseName;
  m_sql_num = sql_num;
  m_thread_num = thread_num;
  m_log_write = log_write;
  m_OPT_LINGER = opt_linger;
  m_TRIGMode = trigmode;
  m_close_log = close_log;
  m_actormodel = actor_model;
}

void WebServer::sql_pool() {
  m_connPool = connection_pool::GetInstance();
  m_connPool->init("127.0.0.1", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);
  users->initmysql_result(m_connPool);
}

void WebServer::thread_pool() {
  m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

void WebServer::trig_mode() {
  if (m_TRIGMode == 0) {
    // LT + LT
    m_LISTENTrigmode = 0;
    m_CONNTrigmode = 0;
  } else if (m_TRIGMode == 1) {
    // LT + ET
    m_LISTENTrigmode = 0;
    m_CONNTrigmode = 1;
  } else if (m_TRIGMode == 2) {
    // ET + LT
    m_LISTENTrigmode = 1;
    m_CONNTrigmode = 0;
  } else if (m_TRIGMode) {
    // ET + ET
    m_LISTENTrigmode = 1;
    m_CONNTrigmode = 1;
  }
}

void WebServer::eventListen() {
  m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
  assert(m_listenfd >= 0);

  if (m_OPT_LINGER == 0) {
    struct linger tmp = {0, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
  } else if (m_OPT_LINGER == 1) {
    struct linger tmp = {1, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
  }

  int ret = 0;
  struct sockaddr_in address;
  bzero(&address, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(m_port);

  int flag = 1;
  setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
  ret = bind(m_listenfd, (struct sockaddr *) &address, sizeof(address));
  assert(ret >= 0);
  ret = listen(m_listenfd, 5);
  assert(ret >= 0);

  utils.init(TIMESLOT);

  epoll_event events[MAX_EVENT_NUMBER];
  m_epollfd = epoll_create(5);
  assert(m_epollfd != -1);

  utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
  http_conn::m_epollfd = m_epollfd;

  ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
  assert(ret != -1);
  utils.setnonblocking(m_pipefd[1]);
  utils.addfd(m_epollfd, m_pipefd[0], false, 0);
  utils.addsig(SIGPIPE, SIG_IGN);
  utils.addsig(SIGALRM, utils.sig_handler, false);
  utils.addsig(SIGTERM, utils.sig_handler, false);

  alarm(TIMESLOT);

  Utils::u_pipefd = m_pipefd;
  Utils::u_epollfd = m_epollfd;
}

void WebServer::eventLoop() {
  bool timeout = false;
  bool stop_server = false;
  while (!stop_server) {
    int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
    cout << "epoll_wait return" << endl;
    if (number < 0 && errno != EINTR) {
      break;
    }
    cout << "event number = " << number << endl;
    for (int i = 0; i < number; ++i) {
      int sockfd = events[i].data.fd;
      if (sockfd == m_listenfd) {
        // 如果是新连接，要处理
        cout << "new connect" << endl;
        bool flag = dealclientdata();
        if (flag == false) continue;
      } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        // 如果服务器关闭了连接，移除定时器（EPOLLRDHUP TCP连接被对方关闭 | EPOLLHUP 挂起 | EPOLLERR 错误）
        cout << "connection closed" << endl;
        util_timer *timer = users_timer[sockfd].timer;
        deal_timer(timer, sockfd);
      } else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)) {
        // 如果有信号来了
        cout << "deal signal" << endl;
        bool flag = dealwithsignal(timeout, stop_server);
      } else if (events[i].events & EPOLLIN) {
        // 如果有可读信号
        cout << "epoll in" << endl;
        dealwithread(sockfd);
      } else if (events[i].events & EPOLLOUT) {
        // 如果有可写信号
        cout << "epoll out" << endl;
        dealwithwrite(sockfd);
      }
    }
    if (timeout) {
      utils.timer_handler();
      timeout = false;
    }
    cout << "epoll_waiting\n" << endl;
  }
}

bool WebServer::dealclientdata() {
  struct sockaddr_in client_address;
  socklen_t client_addrlength = sizeof(client_address);
  if (m_LISTENTrigmode == 0) {
    // LT模式
    int connfd = accept(m_listenfd, (struct sockaddr *) &client_address, &client_addrlength);
    if (connfd < 0) {
      return false;
    }
    if (http_conn::m_user_count >= MAX_FD) {
      return false;
    }
    timer(connfd, client_address);
  } else {
    // ET模式
    while (1) {
      int connfd = accept(m_listenfd, (struct sockaddr *) &client_address, &client_addrlength);
      if (connfd < 0) {
        break;
      }
      if (http_conn::m_user_count >= MAX_FD) {
        break;
      }
      timer(connfd, client_address);
    }
    return false;
  }
  return true;
}

void WebServer::timer(int connfd, struct sockaddr_in client_address) {
  users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode,
                     m_close_log, m_user, m_passWord, m_databaseName);
  cout << "connfd = " << connfd << " init" << endl;

  users_timer[connfd].address = client_address;
  users_timer[connfd].sockfd = connfd;
  util_timer *timer = new util_timer;
  timer->user_data = &users_timer[connfd];
  timer->cb_func = cb_func;
  time_t cur = time(NULL);
  timer->expire = cur + 3 * TIMESLOT;
  users_timer[connfd].timer = timer;
  utils.m_timer_lst.add_timer(timer);
}

void WebServer::deal_timer(util_timer *timer, int sockfd) {
  timer->cb_func(&users_timer[sockfd]);
  if (timer) {
    utils.m_timer_lst.del_timer(timer);
  }
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server) {
  int ret = 0;
  int sig;
  char signals[1024];
  ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
  if (ret == -1) {
    return false;
  } else if (ret == 0) {
    return false;
  } else {
    for (int i = 0; i < ret; ++i) {
      switch (signals[i]) {
        case SIGALRM: {
          timeout = true;
          break;
        }
        case SIGTERM: {
          stop_server = true;
          break;
        }
      }
    }
  }
  return true;
}

void WebServer::dealwithread(int sockfd) {
  cout << "处理读事件, actormodel = " << m_actormodel << endl;
  util_timer *timer = users_timer[sockfd].timer;
  if (m_actormodel == 1) {
    if (timer) {
      adjust_timer(timer);
    }

    m_pool->append(users + sockfd, 0);

    while (true) {
      if (users[sockfd].improv == 1) {
        if (users[sockfd].timer_flag == 1) {
          deal_timer(timer, sockfd);
          users[sockfd].timer_flag = 0;
        }
        users[sockfd].improv = 0;
        break;
      }
    }
  } else {
    if (users[sockfd].read_once()) {
      m_pool->append_p(users + sockfd);
      if (timer) {
        adjust_timer(timer);
      }
    } else {
      deal_timer(timer, sockfd);
    }
  }
}

void WebServer::adjust_timer(util_timer *timer) {
  time_t cur = time(NULL);
  timer->expire = cur + 3 * TIMESLOT;
  utils.m_timer_lst.adjust_timer(timer);
}

void WebServer::dealwithwrite(int sockfd) {
  util_timer *timer = users_timer[sockfd].timer;
  if (m_actormodel == 1) {
    if (timer) {
      adjust_timer(timer);
    }
    m_pool->append(users + sockfd, 1);

    while (true) {
      if (users[sockfd].improv == 1) {
        if (users[sockfd].timer_flag) {
          deal_timer(timer, sockfd);
          users[sockfd].timer_flag = 0;
        }
        users[sockfd].improv = 0;
        break;
      }
    }
  } else {
    if (users[sockfd].write()) {
      if (timer) {
        adjust_timer(timer);
      }
    } else {
      deal_timer(timer, sockfd);
    }
  }
}