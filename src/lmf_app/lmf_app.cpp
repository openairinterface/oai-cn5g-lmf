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

#include "lmf_app.hpp"
#include "lmf_nrf.hpp"

#include "lmf_client.hpp"
#include "logger.hpp"
#include <unistd.h>

#include "LocationData.h"
#include "N1MessageContainer.h"
#include "N1MessageClass.h"
#include "N1N2MessageTransferReqData.h"
#include "N2InformationTransferReqData.h"
#include "UeN1N2InfoSubscriptionCreateData.h"
#include "UeN1N2InfoSubscriptionCreatedData.h"
#include "RefToBinaryData.h"
#include "ProblemDetails.h"
#include "conversions.hpp"
#include "mime_parser.hpp"
#include "iostream"
#include <iterator>
#include <string>

using namespace std;
using namespace oai::lmf::app;
using namespace oai::lmf_server::model;

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
  try {
    lmf_nrf_inst = new lmf_nrf(ev);
    // Register to NRF
    if (lmf_cfg.register_nrf) {
      lmf_nrf_inst->register_to_nrf();
    }
    Logger::lmf_app().info("NRF TASK Created ");
  } catch (std::exception& e) {
    Logger::lmf_app().error("Cannot create NRF TASK: %s", e.what());
    throw;
  }

  if (lmf_cfg.request_trp_info) {
    NRPPA_PDU_t* nrppaPdu = new NRPPA_PDU_t();
    build_trp_information_request_nrppa_pdu(nrppaPdu);

    // xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppaPdu);

    asn_encode_to_new_buffer_result_t nrppaPduEnc = asn_encode_to_new_buffer(
        0, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_NRPPA_PDU, nrppaPdu);

    if (nrppaPduEnc.result.encoded == -1) {
      Logger::lmf_app().error(
          "Could not encode (at %s)\n",
          nrppaPduEnc.result.failed_type ?
              nrppaPduEnc.result.failed_type->name :
              "unknown");
    } else {
      std::string amf_uri  = {};
      std::string method   = "POST";
      std::string response = {};
      amf_uri              = "http://" +
                std::string(inet_ntoa(
                    *((struct in_addr*) &lmf_cfg.amf_addr.ipv4_addr))) +
                ":" + std::to_string(lmf_cfg.amf_addr.port) + "/namf-comm/" +
                lmf_cfg.amf_addr.api_version + "/non-ue-n2-messages/transfer";
      Logger::lmf_app().debug("AMF's URI %s", amf_uri.c_str());

      std::string nrppaMsgStr(
          (char*) nrppaPduEnc.buffer, nrppaPduEnc.result.encoded);
      std::string nrppaMsgHex = {};
      conv::convert_string_2_hex(nrppaMsgStr, nrppaMsgHex);

      RefToBinaryData ngapData = {};
      ngapData.setContentId(N2_NRPPa_CONTENT_ID);

      NgapIeType ngapIeType = {};
      ngapIeType.setEnumValue(NgapIeType_anyOf::eNgapIeType_anyOf::NRPPA_PDU);

      N2InfoContent n2InfoContent = {};
      n2InfoContent.setNgapIeType(ngapIeType);
      n2InfoContent.setNgapData(ngapData);

      NrppaInformation nrppaInformation = {};
      nrppaInformation.setNfId(
          lmf_nrf_inst->lmf_nf_profile.get_nf_instance_id());
      nrppaInformation.setNrppaPdu(n2InfoContent);

      N2InformationClass n2InformationClass = {};
      n2InformationClass.setEnumValue(
          N2InformationClass_anyOf::eN2InformationClass_anyOf::NRPPA);
      N2InfoContainer n2InfoContainer = {};
      n2InfoContainer.setN2InformationClass(n2InformationClass);
      n2InfoContainer.setNrppaInfo(nrppaInformation);

      N2InformationTransferReqData n2InformationTransferReqData = {};
      n2InformationTransferReqData.setN2Information(n2InfoContainer);

      nlohmann::json n2InformationTransferReqData_json;
      to_json(n2InformationTransferReqData_json, n2InformationTransferReqData);

      std::string body      = {};
      std::string json_part = {};
      json_part             = n2InformationTransferReqData_json.dump();

      mime_parser::create_multipart_related_content(
          body, json_part, CURL_MIME_BOUNDARY, nrppaMsgHex,
          multipart_related_content_part_e::NGAP);

      lmf_client_inst->curl_http_client(amf_uri, method, body, response, true);

      Logger::lmf_app().info("Response from AMF: %s", response.c_str());
    }
  }
  Logger::lmf_app().startup("Started");
}

//------------------------------------------------------------------------------
lmf_app::~lmf_app() {
  Logger::lmf_app().debug("Delete LMF_APP instance...");
}

// 3GPP TS 29.518 version 16.4.0 Release 16 / 5.2.2.3.4 N1N2MessageUnSubscribe
void lmf_app::n1_n2_message_unsubscribe(
    nlohmann::json& json_data, Pistache::Http::Code& code, std::string ueSupi,
    std::string n1n2NotifySubscriptionId) {
  std::string amf_uri  = {};
  std::string method   = "DELETE";
  std::string response = {};
  // 1. DELETE
  // ./namf_comm/v1/ue_contexts/{ueContextId}/n1-n2-messages/subscriptions/{subscriptionId}
  amf_uri =
      "http://" +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.amf_addr.ipv4_addr))) +
      ":" + std::to_string(lmf_cfg.amf_addr.port) + "/namf-comm/" +
      lmf_cfg.amf_addr.api_version + "/ue-contexts/" + ueSupi +
      "/n1-n2-messages/subscriptions/" + n1n2NotifySubscriptionId;
  Logger::lmf_app().debug("AMF's URI %s", amf_uri.c_str());

  // 2. 204 No Content
  lmf_client_inst->curl_http_client(amf_uri, method, "", response, false);

  Logger::lmf_app().info("Response from AMF: %s", response.c_str());
}

// 3GPP TS 29.518 version 16.4.0 Release 16 / 5.2.2.3.3 N1N2MessageSubscribe
std::string lmf_app::n1_n2_message_subscribe(
    nlohmann::json& json_data, Pistache::Http::Code& code, std::string ueSupi) {
  std::string amf_uri  = {};
  std::string method   = "POST";
  std::string response = {};
  // 1. POST
  // ./namf_comm/v1/ue_contexts/{ueContextld}/nl-n2-messages/subscriptions
  // (UeN1N2lnfoSubscriptionCreateData)
  amf_uri =
      "http://" +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.amf_addr.ipv4_addr))) +
      ":" + std::to_string(lmf_cfg.amf_addr.port) + "/namf-comm/" +
      lmf_cfg.amf_addr.api_version + "/ue-contexts/" + ueSupi +
      "/n1-n2-messages/subscriptions";
  Logger::lmf_app().debug("AMF's URI %s", amf_uri.c_str());

  N2InformationClass n2InformationClass = {};
  n2InformationClass.setEnumValue(
      N2InformationClass_anyOf::eN2InformationClass_anyOf::NRPPA);

  // 5.2.2.3.6 N2InfoNotify n2InfoNotifyUri
  std::string n2NotifyCallbackUri =
      "http://" +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.sbi.addr4))) + ":" +
      std::to_string(
          lmf_cfg.use_http2 ? lmf_cfg.sbi_http2_port : lmf_cfg.sbi.port) +
      "/nlmf-n2info-notify/v2/nrppa/callback/" + ueSupi;

  std::string nfId = lmf_nrf_inst->lmf_instance_id;

  // 6.1.6.2.12 Type: UeN1N2InfoSubscriptionCreateData
  UeN1N2InfoSubscriptionCreateData ueN1N2InfoSubscriptionCreateData = {};
  ueN1N2InfoSubscriptionCreateData.setN2InformationClass(n2InformationClass);
  ueN1N2InfoSubscriptionCreateData.setN2NotifyCallbackUri(n2NotifyCallbackUri);
  ueN1N2InfoSubscriptionCreateData.setNfId(nfId);

  nlohmann::json ueN1N2InfoSubscriptionCreateData_json;
  to_json(
      ueN1N2InfoSubscriptionCreateData_json, ueN1N2InfoSubscriptionCreateData);

  // 2. 201 Created (UeN1MessageSubscriptionCreatedData)
  lmf_client_inst->curl_http_client(
      amf_uri, method, ueN1N2InfoSubscriptionCreateData_json.dump().c_str(),
      response, false);

  Logger::lmf_app().info("Response from AMF: %s", response.c_str());

  // 6.1.6.2.13 Type: UeN1N2InfoSubscriptionCreatedData
  nlohmann::json ueN1N2InfoSubscriptionCreatedData_json;
  UeN1N2InfoSubscriptionCreatedData ueN1N2InfoSubscriptionCreatedData = {};

  ueN1N2InfoSubscriptionCreatedData_json = nlohmann::json::parse(response);
  from_json(
      ueN1N2InfoSubscriptionCreatedData_json,
      ueN1N2InfoSubscriptionCreatedData);

  return ueN1N2InfoSubscriptionCreatedData.getN1n2NotifySubscriptionId();
}

void lmf_app::handle_determine_location(
    const InputData& inputData, nlohmann::json& json_data,
    Pistache::Http::Code& code, uint8_t http_version) {
  Logger::lmf_app().info("Handle Determin Location Request");
  std::string ueSupi = inputData.getSupi();
  if (is_supi_2_context(ueSupi)) {
    Logger::lmf_app().warn(
        "Already ongoing determine location for supi: '%s'", ueSupi);
    ProblemDetails problemDetails;
    nlohmann::json problemDetails_json = {};
    problemDetails.setCause("INTERNAL_SERVER_ERROR");
    problemDetails.setStatus(500);
    std::string errorMsg =
        "Already ongoing determine location for supi: " + ueSupi;
    errorMsg += "\n";
    problemDetails.setDetail(errorMsg);
    to_json(problemDetails_json, problemDetails);

    code      = Pistache::Http::Code::Internal_Server_Error;
    json_data = problemDetails_json;
    return;
  }

  try {
    auto subId = n1_n2_message_subscribe(json_data, code, ueSupi);
  } catch (std::exception& e) {
    Logger::lmf_app().error("N2N2InfoSubscribe failed: %s", e.what());

    ProblemDetails problemDetails;
    nlohmann::json problemDetails_json = {};
    problemDetails.setCause("INTERNAL_SERVER_ERROR");
    problemDetails.setStatus(500);
    std::string errorMsg = "N2N2InfoSubscribe failed: ";
    errorMsg += e.what();
    errorMsg += "\n";
    problemDetails.setDetail(errorMsg);
    to_json(problemDetails_json, problemDetails);

    code      = Pistache::Http::Code::Internal_Server_Error;
    json_data = problemDetails_json;

    return;
  }

  json_data["supi"] = ueSupi;

  LPP_Message_t* lppMsg = new LPP_Message_t();
  build_request_location_lpp_pdu(lppMsg);

  asn_encode_to_new_buffer_result_t lppMsgEnc = asn_encode_to_new_buffer(
      0, ATS_UNALIGNED_BASIC_PER, &asn_DEF_LPP_Message, lppMsg);
  if (lppMsgEnc.result.encoded == -1) {
    Logger::lmf_app().error(
        "Could not encode (at %s)\n", lppMsgEnc.result.failed_type ?
                                          lppMsgEnc.result.failed_type->name :
                                          "unknown");

    ProblemDetails problemDetails;
    nlohmann::json problemDetails_json = {};
    problemDetails.setCause("INTERNAL_SERVER_ERROR");
    problemDetails.setStatus(500);
    std::string errorMsg = "Could not encode (at ";
    errorMsg +=
        (lppMsgEnc.result.failed_type ? lppMsgEnc.result.failed_type->name :
                                        "unknown");
    errorMsg += ")\n";
    problemDetails.setDetail(errorMsg);
    to_json(problemDetails_json, problemDetails);

    code      = Pistache::Http::Code::Internal_Server_Error;
    json_data = problemDetails_json;
    return;
  }

  NRPPA_PDU_t* nrppaPdu = new NRPPA_PDU_t();
  build_positioning_information_request_nrppa_pdu(nrppaPdu);

  xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppaPdu);

  asn_encode_to_new_buffer_result_t nrppaPduEnc = asn_encode_to_new_buffer(
      0, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_NRPPA_PDU, nrppaPdu);

  if (nrppaPduEnc.result.encoded == -1) {
    Logger::lmf_app().error(
        "Could not encode (at %s)\n", nrppaPduEnc.result.failed_type ?
                                          nrppaPduEnc.result.failed_type->name :
                                          "unknown");

    ProblemDetails problemDetails;
    nlohmann::json problemDetails_json = {};
    problemDetails.setCause("INTERNAL_SERVER_ERROR");
    problemDetails.setStatus(500);
    std::string errorMsg = "Could not encode (at ";
    errorMsg +=
        (nrppaPduEnc.result.failed_type ? nrppaPduEnc.result.failed_type->name :
                                          "unknown");
    errorMsg += ")\n";
    problemDetails.setDetail(errorMsg);
    to_json(problemDetails_json, problemDetails);

    code      = Pistache::Http::Code::Internal_Server_Error;
    json_data = problemDetails_json;
    return;
  }

  std::string amf_uri  = {};
  std::string method   = "POST";
  std::string response = {};
  amf_uri =
      "http://" +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.amf_addr.ipv4_addr))) +
      ":" + std::to_string(lmf_cfg.amf_addr.port) + "/namf-comm/" +
      lmf_cfg.amf_addr.api_version + "/ue-contexts/" + ueSupi +
      "/n1-n2-messages";
  Logger::lmf_app().debug("AMF's URI %s", amf_uri.c_str());

  /**N1MessageContainer n1MessageContainer = {};

  // N1 Message Class
  N1MessageClass lppN1MessageClass = {};
  lppN1MessageClass.setEnumValue(
      N1MessageClass_anyOf::eN1MessageClass_anyOf::LPP);
  n1MessageContainer.setN1MessageClass(lppN1MessageClass);

  // N1 Message Container
  std::string n1MessageDataStr(
      (char*) lppMsgEnc.buffer,
      (char*) (lppMsgEnc.buffer) + lppMsgEnc.result.encoded);
  RefToBinaryData n1MessageData = {};
  n1MessageData.setContentId(n1MessageDataStr);
  n1MessageContainer.setN1MessageContent(n1MessageData);*/

  std::string nrppaMsgStr(
      (char*) nrppaPduEnc.buffer, nrppaPduEnc.result.encoded);
  std::string nrppaMsgHex = {};
  conv::convert_string_2_hex(nrppaMsgStr, nrppaMsgHex);

  RefToBinaryData ngapData = {};
  ngapData.setContentId(N2_NRPPa_CONTENT_ID);

  NgapIeType ngapIeType = {};
  ngapIeType.setEnumValue(NgapIeType_anyOf::eNgapIeType_anyOf::NRPPA_PDU);

  N2InfoContent n2InfoContent = {};
  n2InfoContent.setNgapIeType(ngapIeType);
  n2InfoContent.setNgapData(ngapData);

  NrppaInformation nrppaInformation = {};
  nrppaInformation.setNfId(lmf_nrf_inst->lmf_nf_profile.get_nf_instance_id());
  nrppaInformation.setNrppaPdu(n2InfoContent);

  N2InformationClass n2InformationClass = {};
  n2InformationClass.setEnumValue(
      N2InformationClass_anyOf::eN2InformationClass_anyOf::NRPPA);
  N2InfoContainer n2InfoContainer = {};
  n2InfoContainer.setN2InformationClass(n2InformationClass);
  n2InfoContainer.setNrppaInfo(nrppaInformation);

  N1N2MessageTransferReqData n1n2MessageTransferReqData = {};
  n1n2MessageTransferReqData.setN2InfoContainer(n2InfoContainer);

  nlohmann::json n1n2MessageTransferReq_json;
  to_json(n1n2MessageTransferReq_json, n1n2MessageTransferReqData);

  std::string body      = {};
  std::string json_part = {};
  json_part             = n1n2MessageTransferReq_json.dump();

  mime_parser::create_multipart_related_content(
      body, json_part, CURL_MIME_BOUNDARY, nrppaMsgHex,
      multipart_related_content_part_e::NGAP);

  auto ctx = std::make_shared<LMFContext>(ueSupi);
  set_supi_2_context(inputData.getSupi(), ctx);

  lmf_client_inst->curl_http_client(amf_uri, method, body, response, true);

  Logger::lmf_app().info("Response from AMF: %s", response.c_str());

  json_data = ctx->promise.get_future().get();

  code = Pistache::Http::Code::Ok;
}

bool lmf_app::is_supi_2_context(const string& supi) const {
  std::shared_lock lock(m_supi2ctx);
  return (supi2ctx.count(supi) > 0) && (supi2ctx.at(supi) != nullptr);
}
/*
std::shared_ptr<LMFContext> lmf_app::create_lmf_context(
    const string& supi) {
  std::shared_lock lock(m_supi2ctx);
  if ((supi2ctx.count(supi) > 0) && (supi2ctx.at(supi) != nullptr)) {
    return std::make_shared<LMFContext>(nullptr);
  }
}
*/
std::shared_ptr<LMFContext> lmf_app::supi_2_context(
    const std::string& supi) const {
  std::shared_lock lock(m_supi2ctx);
  return supi2ctx.at(supi);
}

void lmf_app::set_supi_2_context(
    const string& supi, const std::shared_ptr<LMFContext>& lc) {
  std::unique_lock lock(m_supi2ctx);
  supi2ctx[supi] = lc;
}

void lmf_app::del_supi_2_context(const string& supi) {
  std::unique_lock lock(m_supi2ctx);
  supi2ctx.erase(supi);
}

bool lmf_app::handle_n2info_nrppa_notification(
    std::string supi, NRPPA_PDU_t* nrppa, ProblemDetails& problem_details,
    uint8_t& http_code) {
  if (nrppa->present != NRPPA_PDU_PR_successfulOutcome) {
    Logger::lmf_server().error(
        "nrppa->present != NRPPA_PDU_PR_successfulOutcome: %d", nrppa->present);
    return false;
  }

  supi_2_context(supi)->finish();
  del_supi_2_context(supi);

  return true;
}

void lmf_app::build_request_location_lpp_pdu(LPP_Message_t* lppMsg) {
  lppMsg->endTransaction = true;

  lppMsg->transactionID =
      (LPP_TransactionID_t*) calloc(1, sizeof(LPP_TransactionID_t));
  lppMsg->transactionID->initiator         = Initiator_locationServer;
  long transno                             = 10;
  lppMsg->transactionID->transactionNumber = transno;

  lppMsg->lpp_MessageBody =
      (LPP_MessageBody_t*) calloc(1, sizeof(LPP_MessageBody_t));
  lppMsg->lpp_MessageBody->present = LPP_MessageBody_PR_c1;
  lppMsg->lpp_MessageBody->choice.c1 =
      (LPP_MessageBody::LPP_MessageBody_u::LPP_MessageBody__c1*) calloc(
          1, sizeof(LPP_MessageBody::LPP_MessageBody_u::LPP_MessageBody__c1));
  lppMsg->lpp_MessageBody->choice.c1->present =
      LPP_MessageBody__c1_PR_requestLocationInformation;
  lppMsg->lpp_MessageBody->choice.c1->choice.requestLocationInformation =
      (RequestLocationInformation_t*) calloc(
          1, sizeof(RequestLocationInformation_t));
  lppMsg->lpp_MessageBody->choice.c1->choice.requestLocationInformation
      ->criticalExtensions.present =
      RequestLocationInformation__criticalExtensions_PR_c1;
  lppMsg->lpp_MessageBody->choice.c1->choice.requestLocationInformation
      ->criticalExtensions.choice
      .c1 = (RequestLocationInformation::
                 RequestLocationInformation__criticalExtensions::
                     RequestLocationInformation__criticalExtensions_u::
                         RequestLocationInformation__criticalExtensions__c1*)
      calloc(
          1,
          sizeof(
              RequestLocationInformation::
                  RequestLocationInformation__criticalExtensions::
                      RequestLocationInformation__criticalExtensions_u::
                          RequestLocationInformation__criticalExtensions__c1));
  lppMsg->lpp_MessageBody->choice.c1->choice.requestLocationInformation
      ->criticalExtensions.choice.c1->present =
      RequestLocationInformation__criticalExtensions__c1_PR_requestLocationInformation_r9;
  lppMsg->lpp_MessageBody->choice.c1->choice.requestLocationInformation
      ->criticalExtensions.choice.c1->choice.requestLocationInformation_r9 =
      (RequestLocationInformation_r9_IEs_t*) calloc(
          1, sizeof(RequestLocationInformation_r9_IEs_t));
  lppMsg->lpp_MessageBody->choice.c1->choice.requestLocationInformation
      ->criticalExtensions.choice.c1->choice.requestLocationInformation_r9
      ->commonIEsRequestLocationInformation =
      (CommonIEsRequestLocationInformation_t*) calloc(
          1, sizeof(CommonIEsRequestLocationInformation_t));
  lppMsg->lpp_MessageBody->choice.c1->choice.requestLocationInformation
      ->criticalExtensions.choice.c1->choice.requestLocationInformation_r9
      ->commonIEsRequestLocationInformation->locationInformationType =
      LocationInformationType_locationMeasurementsRequired;
  lppMsg->lpp_MessageBody->choice.c1->choice.requestLocationInformation
      ->criticalExtensions.choice.c1->choice.requestLocationInformation_r9
      ->commonIEsRequestLocationInformation->locationCoordinateTypes =
      (LocationCoordinateTypes_t*) calloc(1, sizeof(LocationCoordinateTypes_t));
  lppMsg->lpp_MessageBody->choice.c1->choice.requestLocationInformation
      ->criticalExtensions.choice.c1->choice.requestLocationInformation_r9
      ->commonIEsRequestLocationInformation->locationCoordinateTypes
      ->ellipsoidPoint = true;
  lppMsg->lpp_MessageBody->choice.c1->choice.requestLocationInformation
      ->criticalExtensions.choice.c1->choice.requestLocationInformation_r9
      ->commonIEsRequestLocationInformation->velocityTypes =
      (VelocityTypes_t*) calloc(1, sizeof(VelocityTypes_t));
  lppMsg->lpp_MessageBody->choice.c1->choice.requestLocationInformation
      ->criticalExtensions.choice.c1->choice.requestLocationInformation_r9
      ->commonIEsRequestLocationInformation->velocityTypes->horizontalVelocity =
      true;
}

void lmf_app::build_trp_information_request_nrppa_pdu(NRPPA_PDU_t* nrppaPdu) {
  nrppaPdu->present                  = NRPPA_PDU_PR_initiatingMessage;
  nrppaPdu->choice.initiatingMessage = new InitiatingMessage_t();
  nrppaPdu->choice.initiatingMessage->nrppatransactionID = 10;

  nrppaPdu->choice.initiatingMessage->procedureCode =
      ProcedureCode_id_tRPInformationExchange;
  nrppaPdu->choice.initiatingMessage->criticality = Criticality_reject;
  nrppaPdu->choice.initiatingMessage->value.present =
      InitiatingMessage__value_PR::
          InitiatingMessage__value_PR_TRPInformationRequest;

  TRPInformationRequest_IEs_t* trpList = new TRPInformationRequest_IEs_t();
  trpList->id                          = ProtocolIE_ID_id_TRPList;
  trpList->criticality                 = Criticality_reject;
  trpList->value.present               = TRPInformationRequest_IEs__value_PR::
      TRPInformationRequest_IEs__value_PR_TRPList;

  // Optional if All TRPs to be included
  TRPItem_t* trpItem1 = new TRPItem_t();
  trpItem1->tRP_ID    = 1;
  TRPItem_t* trpItem2 = new TRPItem_t();
  trpItem2->tRP_ID    = 2;
  ASN_SEQUENCE_ADD(&trpList->value.choice.TRPList.list, trpItem1);
  // ASN_SEQUENCE_ADD(&trpList->value.choice.TRPList.list, trpItem2);

  ASN_SEQUENCE_ADD(
      &nrppaPdu->choice.initiatingMessage->value.choice.TRPInformationRequest
           .protocolIEs.list,
      trpList);

  TRPInformationRequest_IEs_t* trpInformationTypeList =
      new TRPInformationRequest_IEs_t();
  trpInformationTypeList->id = ProtocolIE_ID_id_TRPInformationTypeListTRPReq;
  trpInformationTypeList->criticality   = Criticality_reject;
  trpInformationTypeList->value.present = TRPInformationRequest_IEs__value_PR::
      TRPInformationRequest_IEs__value_PR_TRPInformationTypeListTRPReq;

  for (int i = e_TRPInformationTypeItem::TRPInformationTypeItem_nrPCI;
       i <= e_TRPInformationTypeItem::TRPInformationTypeItem_geoCoord; i++) {
    e_TRPInformationTypeItem type = (e_TRPInformationTypeItem) i;
    Logger::lmf_app().info("Adding TRPInformationTypeItem: %d", (int) type);
    TRPInformationTypeItemTRPReq_t* trpInformationTypeItem =
        new TRPInformationTypeItemTRPReq_t();
    trpInformationTypeItem->id = ProtocolIE_ID_id_TRPInformationTypeItem;
    trpInformationTypeItem->criticality = Criticality_reject;
    trpInformationTypeItem->value.present =
        TRPInformationTypeItemTRPReq__value_PR::
            TRPInformationTypeItemTRPReq__value_PR_TRPInformationTypeItem;
    trpInformationTypeItem->value.choice.TRPInformationTypeItem = type;

    ASN_SEQUENCE_ADD(
        &trpInformationTypeList->value.choice.TRPInformationTypeListTRPReq.list,
        trpInformationTypeItem);
  }

  ASN_SEQUENCE_ADD(
      &nrppaPdu->choice.initiatingMessage->value.choice.TRPInformationRequest
           .protocolIEs.list,
      trpInformationTypeList);
}

void lmf_app::build_positioning_information_request_nrppa_pdu(
    NRPPA_PDU_t* nrppaPdu) {
  nrppaPdu->present                  = NRPPA_PDU_PR_initiatingMessage;
  nrppaPdu->choice.initiatingMessage = new InitiatingMessage_t();
  nrppaPdu->choice.initiatingMessage->nrppatransactionID = 10;

  nrppaPdu->choice.initiatingMessage->procedureCode =
      ProcedureCode_id_positioningInformationExchange;
  nrppaPdu->choice.initiatingMessage->criticality = Criticality_reject;
  nrppaPdu->choice.initiatingMessage->value.present =
      InitiatingMessage__value_PR::
          InitiatingMessage__value_PR_PositioningInformationRequest;

  PositioningInformationRequest_IEs_t* positioningInformationRequestIEs =
      new PositioningInformationRequest_IEs_t();
  positioningInformationRequestIEs->id =
      ProtocolIE_ID_id_RequestedSRSTransmissionCharacteristics;
  positioningInformationRequestIEs->criticality = Criticality_ignore;
  positioningInformationRequestIEs->value
      .present = PositioningInformationRequest_IEs__value_PR::
      PositioningInformationRequest_IEs__value_PR_RequestedSRSTransmissionCharacteristics;

  RequestedSRSTransmissionCharacteristics_t*
      requestedSRSTransmissionCharacteristics =
          &positioningInformationRequestIEs->value.choice
               .RequestedSRSTransmissionCharacteristics;
  requestedSRSTransmissionCharacteristics->resourceType =
      RequestedSRSTransmissionCharacteristics__resourceType::
          RequestedSRSTransmissionCharacteristics__resourceType_aperiodic;
  requestedSRSTransmissionCharacteristics->bandwidth.present =
      BandwidthSRS_PR::BandwidthSRS_PR_fR1;
  requestedSRSTransmissionCharacteristics->bandwidth.choice.fR1 =
      BandwidthSRS__fR1::BandwidthSRS__fR1_mHz5;
  ASN_SEQUENCE_ADD(
      &nrppaPdu->choice.initiatingMessage->value.choice
           .PositioningInformationRequest.protocolIEs.list,
      positioningInformationRequestIEs);
}
