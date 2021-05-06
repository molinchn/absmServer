#include<iostream>
#include<string>
#include "config.h"
using namespace std;
int main(int argc, char *argv[]) {

  string user = "root";
  string passwd = "root";
  string database = "mydb";

  Config config;
  config.parse_arg(argc, argv);

  WebServer server;
  // server.init();
  //
  // server.log_write();
  //
  // server.sql_pool();
  //
  // server.thread_pool();
  //
  // server.trig_mode();
  //
  // server.eventListen();
  //
  // server.eventLoop();

  return 0;
}
