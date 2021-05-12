//
// Created by molin on 2021/5/7.
//
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool() {
  m_CurConn = 0;
  m_FreeConn = 0;
}

connection_pool *connection_pool::GetInstance() {
  static connection_pool connPool;
  return &connPool;
}

// 初始化连接
void connection_pool::init(string url,
                           string User,
                           string PassWord,
                           string DBName,
                           int Port,
                           int MaxConn,
                           int close_log) {
  m_url = url;
  m_Port = Port;
  m_User = User;
  m_PassWord = PassWord;
  m_DatabaseName = DBName;
  m_close_log = close_log;

  for (int i = 0; i < MaxConn; ++i) {
    MYSQL *con = NULL;
    con = mysql_init(con);

    if (con == NULL) {
      // LOG_ERROR("MySQL Error");
      exit(1);
    }
    con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

    if (con == NULL) {
      // LOG_ERROR("MySQL Error");
      exit(1);
    }
    connList.push_back(con);
    ++m_FreeConn;
  }
  reserve = sem(m_FreeConn);
  m_MaxConn = m_FreeConn;
}

// 从连接池中获取一个可用连接
MYSQL *connection_pool::GetConnection() {
  MYSQL *con = NULL;

  if (0 == connList.size()) {
    return NULL;
  }
  reserve.wait();

  lock.lock();

  // 从connList中得到连接，从连接池中暂时删除该连接
  con = connList.front();
  connList.pop_front();

  --m_FreeConn;
  ++m_CurConn;

  lock.unlock();
  return con;
}

// 释放连接：把用完的连接放回连接池
bool connection_pool::ReleaseConnection(MYSQL *conn) {
  if (NULL == conn) {
    return false;
  }
  lock.lock();

  connList.push_back(conn);
  ++m_FreeConn;
  --m_CurConn;

  lock.unlock();

  reserve.post();
  return true;
}

void connection_pool::DestroyPool() {
  lock.lock();
  if (connList.size() > 0) {
    list<MYSQL *>::iterator it;
    for (it = connList.begin(); it != connList.end(); ++it) {
      // 依次关掉每个连接
      MYSQL *con = *it;
      mysql_close(con);
    }
    m_CurConn = 0;
    m_FreeConn = 0;
    connList.clear();
  }
  lock.unlock();
}

// 查询空闲连接数
int connection_pool::GetFreeConn() {
  return this->m_FreeConn;
}

connection_pool::~connection_pool() {
  DestroyPool();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool) {
  *SQL = connPool->GetConnection();

  conRAII = *SQL;
  poolRAII = connPool;
}

connectionRAII::~connectionRAII() {
  poolRAII->ReleaseConnection(conRAII);
}

