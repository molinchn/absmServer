//
// Created by molin on 2021/5/6.
//

#ifndef ABSMSERVER_THREADPOOL_THREADPOOL_H_
#define ABSMSERVER_THREADPOOL_THREADPOOL_H_

#include<list>
#include<cstdio>
#include<exception>
#include<pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template<typename T>
class threadpool {
 public:
  threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
  ~threadpool();
  bool append(T *request, int state);
  bool append_p(T *request);

 private:
  static void *worker(void *arg);
  void run();

 private:
  int m_thread_number;
  int m_max_requests;
  pthread_t *m_threads;
  std::list<T *> m_workqueue;
  locker m_queuelocker;
  sem m_queuestat;
  connection_pool *m_connPool;
  int m_actor_model;
};

template<typename T>
threadpool<T>::threadpool(int actor_model,
                          connection_pool *connPool,
                          int thread_number,
                          int max_requests):m_actor_model(actor_model),
                                            m_thread_number(thread_number),
                                            m_max_requests(max_requests),
                                            m_threads(NULL),
                                            m_connPool(connPool) {
  if (thread_number <= 0 || max_requests <= 0) throw std::exception();
  // 以数组的形式定义线程指针
  m_threads = new pthread_t[m_thread_number];
  if (!m_threads) throw std::exception();
  // 对每个线程进行create和detach
  for (int i = 0; i < thread_number; ++i) {
    // 创建线程，线程属性NULL，worker为工作函数，this为参数
    if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
      delete[] m_threads;
      throw std::exception();
    }
    // 非阻塞地地分离子线程，子线程结束后资源会自动回收
    if (pthread_detach(m_threads[i])) {
      delete[] m_threads;
      throw std::exception();
    }
  }
}

template<typename T>
threadpool<T>::~threadpool() {
  delete[] m_threads;
}

template<typename T>
bool threadpool<T>::append(T *request, int state) {
  m_queuelocker.lock();
  // 判断是否会超过队列长度
  if (m_workqueue.size() >= m_max_requests) {
    m_queuelocker.unlock();
    return false;
  }
  // 添加任务
  request->m_state = state;
  m_workqueue.push_back(request);
  m_queuelocker.unlock();
  // 信号量提示有任务
  m_queuestat.post();
  return true;
}

// 这个和上面一个的区别？
template<typename T>
bool threadpool<T>::append_p(T *request) {
  m_queuelocker.lock();
  if (m_workqueue.size() >= m_max_requests) {
    m_queuelocker.unlock();
    return false;
  }
  m_workqueue.push_back(request);
  m_queuelocker.unlock();
  m_queuestat.post();
  return true;
}

template<typename T>
void *threadpool<T>::worker(void *arg) {
  auto *pool = (threadpool *) arg;
  pool->run();
  return pool;
}

template<typename T>
void threadpool<T>::run() {
  while (true) {
    // 争抢任务
    m_queuestat.wait();
    // 运行到这里说明争抢到了任务
    // 对任务队列操作前要上锁
    m_queuelocker.lock();

    if (m_workqueue.empty()) {
      m_queuelocker.unlock();
      continue;
    }
    // 取出这个request
    T *request = m_workqueue.front();
    m_workqueue.pop_front();

    // 操作结束，开锁
    m_queuelocker.unlock();

    if (!request) continue;
    if (m_actor_model == 1) {
      if (request->m_state) {
        if (request->read_once()) {
          request->improv = 1;
          connectionRAII mysqlcon(&request->mysql, m_connPool);
          request->process();
        } else {
          request->improv = 1;
          request->timer_flag = 1;
        }
      }
    } else {
      connectionRAII mysqlcon(&request->mysql, m_connPool);
      request->process();
    }
  }
}
#endif //ABSMSERVER_THREADPOOL_THREADPOOL_H_
