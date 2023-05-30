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

/*! \file lmf_nrf.cpp
 \brief
 \author  Jian Yang, Fengjiao He, Hongxin Wang, Tien-Thinh NGUYEN
 \company Eurecom
 \date 2020
 \email:
 */

#include "lmf_nrf.hpp"
#include "lmf_app.hpp"
#include "lmf_client.hpp"
#include "lmf_profile.hpp"
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <pistache/http.h>
#include <pistache/mime.h>
#include <stdexcept>

#include "lmf.h"
#include "logger.hpp"

using namespace config;
// using namespace lmf;
using namespace oai::lmf::app;
using namespace boost::placeholders;

using json = nlohmann::json;

extern lmf_config lmf_cfg;
extern lmf_nrf* lmf_nrf_inst;
lmf_client* lmf_client_instance = nullptr;

//------------------------------------------------------------------------------
lmf_nrf::lmf_nrf(lmf_event& ev) : m_event_sub(ev) {}
//---------------------------------------------------------------------------------------------
void lmf_nrf::get_lmf_api_root(std::string& api_root) {
  api_root =
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.nrf_addr.ipv4_addr))) +
      ":" + std::to_string(lmf_cfg.nrf_addr.port) + NNRF_NFM_BASE +
      lmf_cfg.nrf_addr.api_version;
}

//---------------------------------------------------------------------------------------------
void lmf_nrf::generate_lmf_profile(
    lmf_profile& lmf_nf_profile, std::string& lmf_instance_id) {
  // TODO: remove hardcoded values
  lmf_nf_profile.set_nf_instance_id(lmf_instance_id);
  lmf_nf_profile.set_nf_instance_name("OAI-LMF");
  lmf_nf_profile.set_nf_type("LMF");
  lmf_nf_profile.set_nf_status("REGISTERED");
  lmf_nf_profile.set_nf_heartBeat_timer(50);
  lmf_nf_profile.set_nf_priority(1);
  lmf_nf_profile.set_nf_capacity(100);
  // lmf_nf_profile.set_fqdn(lmf_cfg.fqdn);
  lmf_nf_profile.add_nf_ipv4_addresses(lmf_cfg.sbi.addr4);  // N4's Addr

  // LMF info (Hardcoded for now)

  lmf_info_t lmf_info_item;
  lmf_info_item.lmfId = lmf_instance_id;
  lmf_info_item.servingClientTypes.push_back(
      ExternalClientType_t::EMERGENCY_SERVICES);
  lmf_info_item.servingClientTypes.push_back(
      ExternalClientType_t::VALUE_ADDED_SERVICES);
  lmf_info_item.servingClientTypes.push_back(
      ExternalClientType_t::PLMN_OPERATOR_SERVICES);
  lmf_info_item.servingClientTypes.push_back(
      ExternalClientType_t::LAWFUL_INTERCEPT_SERVICES);
  lmf_info_item.servingClientTypes.push_back(
      ExternalClientType_t::PLMN_OPERATOR_BROADCAST_SERVICES);
  lmf_info_item.servingClientTypes.push_back(
      ExternalClientType_t::PLMN_OPERATOR_OM);
  lmf_info_item.servingClientTypes.push_back(
      ExternalClientType_t::PLMN_OPERATOR_ANONYMOUS_STATISTICS);
  lmf_info_item.servingClientTypes.push_back(
      ExternalClientType_t::PLMN_OPERATOR_TARGET_MS_SERVICE_SUPPORT);
  lmf_info_item.servingAccessTypes.push_back(AccessType_t::_3GPP_ACCESS);
  lmf_info_item.servingAccessTypes.push_back(AccessType_t::NON_3GPP_ACCESS);
  lmf_info_item.servingAnNodeTypes.push_back(AnNodeType_t::GNB);
  lmf_info_item.servingAnNodeTypes.push_back(AnNodeType_t::NG_ENB);
  lmf_info_item.servingRatTypes.push_back(RatType_t::NR);
  lmf_info_item.servingRatTypes.push_back(RatType_t::EUTRA);
  lmf_info_item.servingRatTypes.push_back(RatType_t::WLAN);
  lmf_nf_profile.set_lmf_info(lmf_info_item);

  lmf_nf_profile.display();
}
//---------------------------------------------------------------------------------------------
void lmf_nrf::register_to_nrf() {
  // generate UUID
  lmf_instance_id              = to_string(boost::uuids::random_generator()());
  nlohmann::json response_data = {};

  // Generate NF Profile
  generate_lmf_profile(lmf_nf_profile, lmf_instance_id);

  // Send NF registeration request
  std::string lmf_api_root = {};
  std::string response     = {};
  std::string method       = {"PUT"};
  get_lmf_api_root(lmf_api_root);
  std::string remoteUri = lmf_api_root + LMF_NF_REGISTER_URL + lmf_instance_id;
  nlohmann::json json_data = {};
  lmf_nf_profile.to_json(json_data);

  Logger::lmf_nrf().info("Sending NF registeration request");
  lmf_client_instance->curl_http_client(
      remoteUri, method, json_data.dump().c_str(), response, false);

  try {
    response_data = nlohmann::json::parse(response);
    response_data = nlohmann::json::parse(response);
    if (response.find("REGISTERED") != 0) {
      start_event_nf_heartbeat(remoteUri);
    }
  } catch (nlohmann::json::exception& e) {
    Logger::lmf_nrf().info("NF registeration procedure failed");
  }
}
//---------------------------------------------------------------------------------------------
void lmf_nrf::start_event_nf_heartbeat(std::string& remoteURI) {
  // get current time
  uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
  struct itimerspec its;
  its.it_value.tv_sec  = HEART_BEAT_TIMER;  // seconds
  its.it_value.tv_nsec = 0;                 // 100 * 1000 * 1000; //100ms
  const uint64_t interval =
      its.it_value.tv_sec * 1000 +
      its.it_value.tv_nsec / 1000000;  // convert sec, nsec to msec

  task_connection = m_event_sub.subscribe_task_nf_heartbeat(
      boost::bind(&lmf_nrf::trigger_nf_heartbeat_procedure, this, _1), interval,
      ms + interval);
}
//---------------------------------------------------------------------------------------------
void lmf_nrf::trigger_nf_heartbeat_procedure(uint64_t ms) {
  _unused(ms);
  oai::lmf_server::model::PatchItem patch_item = {};
  std::vector<oai::lmf_server::model::PatchItem> patch_items;
  //{"op":"replace","path":"/nfStatus", "value": "REGISTERED"}
  oai::lmf_server::model::PatchOperation patch_operation;
  patch_operation.setEnumValue(oai::lmf_server::model::PatchOperation_anyOf::
                                   ePatchOperation_anyOf::REPLACE);
  patch_item.setOp(patch_operation);
  patch_item.setPath("/nfStatus");
  patch_item.setValue("REGISTERED");
  patch_items.push_back(patch_item);
  Logger::lmf_nrf().info("Sending NF heartbeat request");

  std::string response     = {};
  std::string method       = {"PATCH"};
  nlohmann::json json_data = nlohmann::json::array();
  for (auto i : patch_items) {
    nlohmann::json item = {};
    to_json(item, i);
    json_data.push_back(item);
  }

  std::string lmf_api_root = {};
  get_lmf_api_root(lmf_api_root);
  std::string remoteUri = lmf_api_root + LMF_NF_REGISTER_URL + lmf_instance_id;
  lmf_client_instance->curl_http_client(
      remoteUri, method, json_data.dump().c_str(), response, false);
  if (!response.empty()) task_connection.disconnect();
}
