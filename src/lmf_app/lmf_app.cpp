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

#include "LocationData.h"
#include "N1MessageContainer.h"
#include "N1MessageClass.h"
#include "N1N2MessageTransferReqData.h"
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

void lmf_app::handle_determine_location(
    const InputData& inputData, nlohmann::json& json_data,
    Pistache::Http::Code& code, uint8_t http_version) {
  Logger::lmf_app().info("Handle Determin Location Request");
  std::string ueSupi = inputData.getSupi();
  json_data["supi"]  = ueSupi;

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
      0, ATS_UNALIGNED_BASIC_PER, &asn_DEF_NRPPA_PDU, nrppaPdu);
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
  ngapData.setContentId("n2msg");

  NgapIeType ngapIeType = {};
  ngapIeType.setEnumValue(NgapIeType_anyOf::eNgapIeType_anyOf::NRPPA_PDU);

  N2InfoContent n2InfoContent = {};
  n2InfoContent.setNgapIeType(ngapIeType);
  n2InfoContent.setNgapData(ngapData);

  NrppaInformation nrppaInformation = {};
  lmf_info_t lmf_info;
  lmf_nrf_inst->lmf_nf_profile.get_lmf_info(lmf_info);
  nrppaInformation.setNfId(lmf_info.lmfId);
  nrppaInformation.setNrppaPdu(n2InfoContent);

  N2InfoContainer n2InfoContainer = {};
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

  lmf_client_inst->curl_http_client(amf_uri, method, body, response);

  Logger::lmf_app().info("Response from AMF: %s", response.c_str());
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

void lmf_app::build_positioning_information_request_nrppa_pdu(
    NRPPA_PDU_t* nrppaPdu) {
  nrppaPdu->present                  = NRPPA_PDU_PR_initiatingMessage;
  nrppaPdu->choice.initiatingMessage = new InitiatingMessage_t();
  nrppaPdu->choice.initiatingMessage->nrppatransactionID = 10;
  nrppaPdu->choice.initiatingMessage->criticality        = Criticality_reject;
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