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
#include "lmf_config_yaml.hpp"

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

using namespace oai::lmf::config;

lmf_config lmf_cfg;
lmf_app* lmf_app_inst              = nullptr;
LMFApiServer* api_server           = nullptr;
lmf_http2_server* lmf_api_server_2 = nullptr;
task_manager* tm_inst              = nullptr;
std::unique_ptr<oai::config::lmf_config_yaml> lmf_cfg_yaml;

//------------------------------------------------------------------------------
void my_app_signal_handler(int s) {
  // Setting log level arbitrarly to debug to show the whole
  // shutdown procedure in the logs even in case of off-logging
  Logger::set_level(spdlog::level::debug);
  Logger::system().info("Caught signal %d", s);

  // Stop on-going tasks
  if (api_server) {
    api_server->shutdown();
  }
  if (lmf_api_server_2) {
    lmf_api_server_2->stop();
  }

  Logger::system().debug("Freeing Allocated memory...");
  // Delete instances
  if (api_server) {
    delete api_server;
    api_server = nullptr;
  }
  if (lmf_api_server_2) {
    delete lmf_api_server_2;
    lmf_api_server_2 = nullptr;
  }
  Logger::system().debug("LMF API Server memory done");

  if (tm_inst) {
    delete tm_inst;
    tm_inst = nullptr;
  }
  Logger::system().debug("Stopped the LMF Task Manager.");

  if (lmf_app_inst) {
    delete lmf_app_inst;
    lmf_app_inst = nullptr;
  }

  Logger::system().debug("LMF APP memory done");
  Logger::system().info("Freeing allocated memory done");
  std::this_thread::sleep_for(3s);
  Logger::system().info("Bye.");
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

  std::signal(SIGTERM, my_app_signal_handler);
  std::signal(SIGINT, my_app_signal_handler);

  // Event subsystem
  lmf_event ev;

  // Config
  std::string conf_file_name = Options::getlibconfigConfig();
  Logger::system().debug("Parsing the configuration file (YAML).");
  lmf_cfg_yaml = std::make_unique<oai::config::lmf_config_yaml>(
      conf_file_name, Options::getlogStdout(), Options::getlogRotFilelog());
  if (!lmf_cfg_yaml->init()) {
    Logger::system().error("Reading the configuration failed. Exiting.");
    return 1;
  }
  lmf_cfg_yaml->pre_process();
  lmf_cfg_yaml->display();
  // Convert from YAML to internal structure
  lmf_cfg_yaml->to_lmf_config(lmf_cfg);
  lmf_cfg.display();

  Logger::set_level(lmf_cfg.log_level);

  // LMF application layer
  lmf_app_inst = new lmf_app(Options::getlibconfigConfig(), ev);

  // Task Manager
  tm_inst = new task_manager(ev);
  std::thread task_manager_thread(&task_manager::run, tm_inst);

  // PID file
  // Currently hard-coded value. TODO: add as config option.
  string pid_file_name =
      oai::utils::get_exe_absolute_path("/var/run", lmf_cfg.instance);
  if (!oai::utils::is_pid_file_lock_success(pid_file_name.c_str())) {
    Logger::lmf_server().error(
        "Lock PID file %s failed\n", pid_file_name.c_str());
    exit(-EDEADLK);
  }

  if (!lmf_cfg.use_http2) {
    // LMF Pistache API server (HTTP1)
    Pistache::Address addr(
        std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.sbi.addr4))),
        Pistache::Port(lmf_cfg.sbi.port));
    api_server = new LMFApiServer(addr, lmf_app_inst);
    api_server->init(lmf_cfg.http_threads_count);
    std::thread lmf_manager(&LMFApiServer::start, api_server);
    lmf_manager.join();
  } else {
    // LMF NGHTTP API server (HTTP2)
    lmf_api_server_2 = new lmf_http2_server(
        oai::utils::conv::toString(lmf_cfg.sbi.addr4), lmf_cfg.sbi_http2_port,
        lmf_cfg.http_threads_count, lmf_app_inst);
    std::thread lmf_http2_manager(&lmf_http2_server::start, lmf_api_server_2);
    lmf_http2_manager.join();
  }

  FILE* fp             = NULL;
  std::string filename = fmt::format("/tmp/lmf_{}.status", getpid());
  fp                   = fopen(filename.c_str(), "w+");
  fprintf(fp, "STARTED\n");
  fflush(fp);
  fclose(fp);

  pause();
  return 0;
}
