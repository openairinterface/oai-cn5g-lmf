/*
 * Copyright (c) 2017 Sprint
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "lmf-api-server.h"
#include "lmf-http2-server.h"
#include "lmf_app.hpp"
#include "lmf_config.hpp"
#include "logger.hpp"
#include "options.hpp"
#include "pid_file.hpp"

#include "pistache/endpoint.h"
#include "pistache/http.h"
#include "pistache/router.h"

#include <iostream>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>  // srand
#include <thread>
#include <unistd.h>  // get_pid(), pause()

using namespace oai::lmf::app;
using namespace util;
using namespace std;

using namespace config;

lmf_config lmf_cfg;
lmf_app* lmf_app_inst              = nullptr;
LMFApiServer* api_server           = nullptr;
lmf_http2_server* lmf_api_server_2 = nullptr;

//------------------------------------------------------------------------------
void my_app_signal_handler(int s) {
  std::cout << "Caught signal " << s << std::endl;
  Logger::system().startup("exiting");
  std::cout << "Freeing Allocated memory..." << std::endl;
  if (api_server) {
    api_server->shutdown();
    delete api_server;
    api_server = nullptr;
  }
  std::cout << "LMF API Server memory done" << std::endl;

  if (lmf_app_inst) {
    delete lmf_app_inst;
    lmf_app_inst = nullptr;
  }

  std::cout << "LMF APP memory done" << std::endl;
  std::cout << "Freeing allocated memory done" << std::endl;

  exit(0);
}

//------------------------------------------------------------------------------
int main(int argc, char** argv) {
  srand(time(NULL));

  // Command line options
  if (!Options::parse(argc, argv)) {
    std::cout << "Options::parse() failed" << std::endl;
    return 1;
  }

  // Logger
  Logger::init("lmf", Options::getlogStdout(), Options::getlogRotFilelog());
  Logger::lmf_server().startup("Options parsed");

  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = my_app_signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  // Event subsystem
  lmf_event ev;

  // Config
  lmf_cfg.load(Options::getlibconfigConfig());
  lmf_cfg.display();

  // LMF application layer
  lmf_app_inst = new lmf_app(Options::getlibconfigConfig(), ev);

  // Task Manager
  task_manager tm(ev);
  std::thread task_manager_thread(&task_manager::run, &tm);

  // PID file
  // Currently hard-coded value. TODO: add as config option.
  string pid_file_name = get_exe_absolute_path("/var/run", lmf_cfg.instance);
  if (!is_pid_file_lock_success(pid_file_name.c_str())) {
    Logger::lmf_server().error(
        "Lock PID file %s failed\n", pid_file_name.c_str());
    exit(-EDEADLK);
  }

  // LMF Pistache API server (HTTP1)
  Pistache::Address addr(
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.sbi.addr4))),
      Pistache::Port(lmf_cfg.sbi.port));
  api_server = new LMFApiServer(addr, lmf_app_inst);
  api_server->init(2);
  std::thread lmf_manager(&LMFApiServer::start, api_server);

  // LMF NGHTTP API server (HTTP2)
  lmf_api_server_2 = new lmf_http2_server(
      conv::toString(lmf_cfg.sbi.addr4), lmf_cfg.sbi_http2_port, lmf_app_inst);
  std::thread lmf_http2_manager(&lmf_http2_server::start, lmf_api_server_2);

  lmf_manager.join();
  lmf_http2_manager.join();

  FILE* fp             = NULL;
  std::string filename = fmt::format("/tmp/lmf_{}.status", getpid());
  fp                   = fopen(filename.c_str(), "w+");
  fprintf(fp, "STARTED\n");
  fflush(fp);
  fclose(fp);

  pause();
  return 0;
}
