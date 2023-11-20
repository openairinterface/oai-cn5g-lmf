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

#include "lmf_non_ue_n2_message_subscription.hpp"

#include <arpa/inet.h>

#include <string>

#include "nlohmann/json.hpp"

#include "lmf_config.hpp"
#include "lmf_client.hpp"
#include "lmf_nrf.hpp"

#include "NonUeN2InfoSubscriptionCreateData.h"
#include "NonUeN2InfoSubscriptionCreatedData.h"

using namespace oai::lmf_server;

// 5.2.2.4.3 NonUeN2InfoUnsubscribe
bool NonUeN2MessageSubscription::unsubscribe() {
  // 1. DELETE
  // ./namf_comm/v1/non-ue-n2-messages/subscriptions/{n2NotifySubscriptionId}
  auto const& amf_uri =
      "http://" +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.amf_addr.ipv4_addr))) +
      ":" + std::to_string(lmf_cfg.amf_addr.port) + "/namf-comm/" +
      lmf_cfg.amf_addr.api_version + "/non-ue-n2-messages/subscriptions/" +
      this->id;

  Logger::lmf_app().debug("AMF's URI %s", amf_uri);

  // 2. 204 No Content
  std::string response;
  lmf_client_inst->curl_http_client(amf_uri, "DELETE", "", response, false);

  Logger::lmf_app().info("Response from AMF: %s", response);

  this->id.clear();

  return true;
}

// 5.2.2.4.2 NonUeN2InfoSubscribe
bool NonUeN2MessageSubscription::subscribe() {
  if (this->is_subscribed()) {
    this->unsubscribe();
  }

  // 1. POST
  // ./namf_comm/v1/non-ue-n2-messages/subscriptions
  // (NonUeN2InfoSubscriptionCreateData)
  auto const& amf_uri =
      "http://" +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.amf_addr.ipv4_addr))) +
      ":" + std::to_string(lmf_cfg.amf_addr.port) + "/namf-comm/" +
      lmf_cfg.amf_addr.api_version + "/non-ue-n2-messages/subscriptions";

  // 5.2.2.3.6 N2InfoNotify n2InfoNotifyUri
  auto const& n2NotifyCallbackUri =
      "http://" +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.sbi.addr4))) + ":" +
      std::to_string(
          lmf_cfg.use_http2 ? lmf_cfg.sbi_http2_port : lmf_cfg.sbi.port) +
      "/nlmf-non-ue-n2info-notify/v2/nrppa/callback";

  Logger::lmf_app().debug("AMF's URI %s", amf_uri);

  // 6.1.6.2.10 Type: NonUeN2InfoSubscriptionCreateData
  model::N2InformationClass n2InformationClass;
  n2InformationClass.setEnumValue(
      model::N2InformationClass_anyOf::eN2InformationClass_anyOf::NRPPA);

  model::NonUeN2InfoSubscriptionCreateData nonUeN2InfoSubscriptionCreateData;
  nonUeN2InfoSubscriptionCreateData.setN2InformationClass(n2InformationClass);
  nonUeN2InfoSubscriptionCreateData.setN2NotifyCallbackUri(n2NotifyCallbackUri);
  nonUeN2InfoSubscriptionCreateData.setNfId(lmf_nrf_inst->lmf_instance_id);

  // 2. 201 Created (UeN1MessageSubscriptionCreatedData)
  std::string response;
  lmf_client_inst->curl_http_client(
      amf_uri, "POST", nlohmann::json(nonUeN2InfoSubscriptionCreateData).dump(),
      response, false);
  Logger::lmf_app().info("Response from AMF: %s", response);

  try {
    // 6.1.6.2.11 Type: NonUeN2InfoSubscriptionCreatedData
    model::NonUeN2InfoSubscriptionCreatedData
        nonUeN2InfoSubscriptionCreatedData{nlohmann::json::parse(response)};
    this->id = nonUeN2InfoSubscriptionCreatedData.getN2NotifySubscriptionId();
  } catch (nlohmann::detail::exception const& ex) {
    Logger::lmf_app().error(
        "subscription failed: respones: '%s', ex: '%s'", response, ex.what());
    return false;
  }

  return true;
}
