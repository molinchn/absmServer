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
  threadpool(connection_pool *connPool, int thread_number = 8, int max_request = 10000);
  ~threadpool();
};

// template<typename T>
// threadpool<T>::threadpool() {
//
// }

#endif //ABSMSERVER_THREADPOOL_THREADPOOL_H_
