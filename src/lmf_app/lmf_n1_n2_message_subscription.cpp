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

#include "lmf_n1_n2_message_subscription.hpp"

#include <arpa/inet.h>

#include <string>
using namespace std::string_literals;

#include "nlohmann/json.hpp"
#include "pistache/http_defs.h"

#include "lmf_config.hpp"
#include "lmf_client.hpp"
#include "lmf_nrf.hpp"

#include "UeN1N2InfoSubscriptionCreateData.h"
#include "UeN1N2InfoSubscriptionCreatedData.h"
#include "ProblemDetails.h"
using namespace oai::lmf_server;

// 3GPP TS 29.518 version 16.4.0 Release 16
// 5.2.2.3.5 N1MessageNotify
// 5.2.2.3.5.5 Using N1MessageNotify in the LCS Event Report, LCS Cancel
// Location and LCS Periodic-Triggered Invoke Procedures

// 5.2.2.3.4 N1N2MessageUnSubscribe
void N1N2MessageSubscription::unsubscribe(
    std::string const& id, std::string const& supi) {
  // 1. DELETE
  // ./namf_comm/v1/ue_contexts/{ueContextId}/n1-n2-messages/subscriptions/{subscriptionId}
  auto const& amf_uri =
      "http://" +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.amf_addr.ipv4_addr))) +
      ":" + std::to_string(lmf_cfg.amf_addr.port) + NAMF_BASE +
      lmf_cfg.amf_addr.api_version + NAMF_N1N2_SUBSCRIBE_BASE + supi +
      NAMF_N1N2_SUBSCRIBE_MESSAGES + NAMF_N1N2_SUBSCRIBE_SUBSCRIPTIONS + "/" +
      id;

  Logger::lmf_app().debug("AMF's URI %s", amf_uri);

  // 2. 204 No Content
  std::string response;
  lmf_client_inst->curl_http_client(amf_uri, "DELETE", "", response, false);
  Logger::lmf_app().debug("Response from AMF: %s", response);

  if (!response.empty()) {
    using namespace Pistache::Http;
    model::ProblemDetails pd;
    pd.setTitle("delete ueN1N2InfoSubscription failed");
    pd.setDetail("amf_uri: '" + amf_uri + "', respone: '" + response);
    Logger::lmf_server().error(pd.getTitle() + ": " + pd.getDetail());
    auto const& reason = nlohmann::json(pd).dump();
    throw HttpError{Code::Internal_Server_Error, reason};
  }
  Logger::lmf_app().info("deleted UeN1N2InfoSubscription %s successfully", id);
}

// 3GPP TS 29.518 version 16.4.0 Release 16 / 5.2.2.3.3 N1N2MessageSubscribe
std::string N1N2MessageSubscription::subscribe(std::string const& supi) {
  // 1. POST
  // ./namf_comm/v1/ue_contexts/{ueContextld}/nl-n2-messages/subscriptions
  // (UeN1N2lnfoSubscriptionCreateData)
  auto const& amf_uri =
      "http://" +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.amf_addr.ipv4_addr))) +
      ":" + std::to_string(lmf_cfg.amf_addr.port) + NAMF_BASE +
      lmf_cfg.amf_addr.api_version + NAMF_N1N2_SUBSCRIBE_BASE + supi +
      NAMF_N1N2_SUBSCRIBE_MESSAGES + NAMF_N1N2_SUBSCRIBE_SUBSCRIPTIONS;

  // 5.2.2.3.6 N2InfoNotify n2InfoNotifyUri
  auto const& n2NotifyCallbackUri =
      "http://" +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.sbi.addr4))) + ":" +
      std::to_string(
          lmf_cfg.use_http2 ? lmf_cfg.sbi_http2_port : lmf_cfg.sbi.port) +
      NLMF_NOTIFY_BASE + lmf_cfg.sbi_api_version + NLMF_NOTIFY_NRPPA_CALLBACK +
      supi;

  Logger::lmf_app().debug("AMF's URI %s", amf_uri);

  // 6.1.6.2.12 Type: UeN1N2InfoSubscriptionCreateData
  model::N2InformationClass n2InformationClass;
  n2InformationClass.setEnumValue(
      model::N2InformationClass_anyOf::eN2InformationClass_anyOf::NRPPA);

  model::UeN1N2InfoSubscriptionCreateData ueN1N2InfoSubscriptionCreateData;
  ueN1N2InfoSubscriptionCreateData.setN2InformationClass(n2InformationClass);
  ueN1N2InfoSubscriptionCreateData.setN2NotifyCallbackUri(n2NotifyCallbackUri);
  ueN1N2InfoSubscriptionCreateData.setNfId(lmf_nrf_inst->lmf_instance_id);

  // 2. 201 Created (UeN1N2InfoSubscriptionCreatedData)
  std::string response;
  lmf_client_inst->curl_http_client(
      amf_uri, "POST", nlohmann::json(ueN1N2InfoSubscriptionCreateData).dump(),
      response, false);
  Logger::lmf_app().debug("Response from AMF: %s", response);

  try {
    // 6.1.6.2.13 Type: UeN1N2InfoSubscriptionCreatedData
    model::UeN1N2InfoSubscriptionCreatedData ueN1N2InfoSubscriptionCreatedData{
        nlohmann::json::parse(response)};
    auto const& id =
        ueN1N2InfoSubscriptionCreatedData.getN1n2NotifySubscriptionId();
    return id;
  } catch (nlohmann::detail::exception const& ex) {
    using namespace Pistache::Http;
    model::ProblemDetails pd;
    pd.setTitle("subscribe ueN1N2InfoSubscription failed");
    pd.setDetail(
        "amf_uri: '" + amf_uri + "', respone: '" + response +
        "', ex: " + ex.what());
    Logger::lmf_server().error(pd.getTitle() + ": " + pd.getDetail());
    auto const& reason = nlohmann::json(pd).dump();
    throw HttpError{Code::Internal_Server_Error, reason};
  }
}
