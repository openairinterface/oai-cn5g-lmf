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

#include <string>

#include "nlohmann/json.hpp"

#include "N2InformationClass.h"
#include "UeN1N2InfoSubscriptionCreateData.h"
#include "UeN1N2InfoSubscriptionCreatedData.h"

#include "lmf_app.hpp"
#include "lmf_nrf.hpp"
#include "lmf_context.hpp"
#include "lmf_config.hpp"
#include "lmf_client.hpp"

using namespace std::string_literals;

using namespace oai::lmf_server::model;
using namespace oai::lmf::app;
using namespace oai::lmf_server::model;
using namespace config;

extern lmf_config lmf_cfg;
extern lmf_nrf* lmf_nrf_inst;
extern lmf_client* lmf_client_inst;

// 3GPP TS 29.518 version 16.4.0 Release 16 / 5.2.2.3.4 N1N2MessageUnSubscribe
bool N1N2MessageSubscription::unsubscribe() {
  // 1. DELETE
  // ./namf_comm/v1/ue_contexts/{ueContextId}/n1-n2-messages/subscriptions/{subscriptionId}
  auto const& amf_uri =
      "http://"s +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.amf_addr.ipv4_addr))) +
      ":" + std::to_string(lmf_cfg.amf_addr.port) + "/namf-comm/"s +
      lmf_cfg.amf_addr.api_version + "/ue-contexts/"s + this->supi +
      "/n1-n2-messages/subscriptions/"s + this->id;

  Logger::lmf_app().debug("AMF's URI %s", amf_uri);

  // 2. 204 No Content
  std::string response;
  lmf_client_inst->curl_http_client(amf_uri, "DELETE", "", response, false);

  Logger::lmf_app().info("Response from AMF: %s", response);

  this->id.clear();

  return true;
}

// 3GPP TS 29.518 version 16.4.0 Release 16 / 5.2.2.3.3 N1N2MessageSubscribe
bool N1N2MessageSubscription::subscribe(std::string supi) {
  if (this->is_subscribed()) {
    this->unsubscribe();
  }
  if (!supi.empty()) {
    this->supi = supi;
  }
  if (this->supi.empty()) {
    return false;
  }

  // 1. POST
  // ./namf_comm/v1/ue_contexts/{ueContextld}/nl-n2-messages/subscriptions
  // (UeN1N2lnfoSubscriptionCreateData)
  auto const& amf_uri =
      "http://"s +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.amf_addr.ipv4_addr))) +
      ":"s + std::to_string(lmf_cfg.amf_addr.port) + "/namf-comm/"s +
      lmf_cfg.amf_addr.api_version + "/ue-contexts/"s + this->supi +
      "/n1-n2-messages/subscriptions"s;

  // 5.2.2.3.6 N2InfoNotify n2InfoNotifyUri
  auto const& n2NotifyCallbackUri =
      "http://"s +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.sbi.addr4))) + ":"s +
      std::to_string(
          lmf_cfg.use_http2 ? lmf_cfg.sbi_http2_port : lmf_cfg.sbi.port) +
      "/nlmf-n2info-notify/v2/nrppa/callback/"s + this->supi;

  Logger::lmf_app().debug("AMF's URI %s", amf_uri);

  // 6.1.6.2.12 Type: UeN1N2InfoSubscriptionCreateData
  N2InformationClass n2InformationClass;
  n2InformationClass.setEnumValue(
      N2InformationClass_anyOf::eN2InformationClass_anyOf::NRPPA);

  UeN1N2InfoSubscriptionCreateData ueN1N2InfoSubscriptionCreateData;
  ueN1N2InfoSubscriptionCreateData.setN2InformationClass(n2InformationClass);
  ueN1N2InfoSubscriptionCreateData.setN2NotifyCallbackUri(n2NotifyCallbackUri);
  ueN1N2InfoSubscriptionCreateData.setNfId(lmf_nrf_inst->lmf_instance_id);

  nlohmann::json ueN1N2InfoSubscriptionCreateData_json{
      ueN1N2InfoSubscriptionCreateData};

  // 2. 201 Created (UeN1MessageSubscriptionCreatedData)
  std::string response;
  lmf_client_inst->curl_http_client(
      amf_uri, "POST", ueN1N2InfoSubscriptionCreateData_json.dump(), response,
      false);

  Logger::lmf_app().info("Response from AMF: %s", response);

  // 6.1.6.2.13 Type: UeN1N2InfoSubscriptionCreatedData
  UeN1N2InfoSubscriptionCreatedData ueN1N2InfoSubscriptionCreatedData{
      nlohmann::json::parse(response)};
  this->id = ueN1N2InfoSubscriptionCreatedData.getN1n2NotifySubscriptionId();

  return true;
}

void LMFContext::finish() {
  LocationData locationData;

  promise.set_value(locationData);
}
