//
// Created by molin on 2021/5/6.
//

#ifndef ABSMSERVER__CONFIG_H_
#define ABSMSERVER__CONFIG_H_

#include "webserver.h"
using namespace std;

class Config {
 public:
  Config();
  ~Config();

  void parse_arg(int argc, char *argv[]);

  int PORT;

  int LOGWrite;

  int TRIGMode;

  int LISTENTrigmode;

  int CONNTrigmode;

  int OPT_LINGER;

  int sql_num;

  int thread_num;

  int close_log;

  int actor_model;
};

#endif //ABSMSERVER__CONFIG_H_