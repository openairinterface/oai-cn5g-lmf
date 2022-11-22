/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this
 * file except in compliance with the License. You may obtain a copy of the
 * License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file lmf_app.cpp
 \brief
 \author  Jian Yang, Fengjiao He, Hongxin Wang, Tien-Thinh NGUYEN
 \company Eurecom
 \date 2021
 \email: contact@openairinterface.org
 */

#include "lmf_app.hpp"
#include "lmf_nrf.hpp"

#include "lmf_client.hpp"
#include "logger.hpp"
#include <unistd.h>
#include "conversions.hpp"
#include "iostream"
#include <iterator>
#include <string>

using namespace std;
using namespace oai::lmf::app;

extern lmf_app* lmf_app_inst;
lmf_client* lmf_client_inst = nullptr;
using namespace config;
extern lmf_config lmf_cfg;
lmf_nrf* lmf_nrf_inst = nullptr;

//------------------------------------------------------------------------------
lmf_app::lmf_app(const std::string& config_file, lmf_event& ev)
    : event_sub(ev) {
  Logger::lmf_app().startup("Starting...");
  try {
    lmf_client_inst = new lmf_client();
  } catch (std::exception& e) {
    Logger::lmf_app().error("Cannot create LMF APP: %s", e.what());
    throw;
  }
  // Register to NRF
  if (lmf_cfg.register_nrf) {
    try {
      lmf_nrf_inst = new lmf_nrf(ev);
      lmf_nrf_inst->register_to_nrf();
      Logger::lmf_app().info("NRF TASK Created ");
    } catch (std::exception& e) {
      Logger::lmf_app().error("Cannot create NRF TASK: %s", e.what());
      throw;
    }
  }
  Logger::lmf_app().startup("Started");
}

//------------------------------------------------------------------------------
lmf_app::~lmf_app() {
  Logger::lmf_app().debug("Delete LMF_APP instance...");
}