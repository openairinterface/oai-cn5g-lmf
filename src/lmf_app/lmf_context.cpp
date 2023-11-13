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

#include "lmf_context.hpp"

#include "lmf.h"
#include "lmf_nrf.hpp"
#include "logger.hpp"
#include "lmf_client.hpp"
#include "conversions.hpp"
#include "mime_parser.hpp"
#include "3gpp_29.518.h"

#include "LocationData.h"
#include "ProblemDetails.h"
#include "NgapIeType.h"
#include "N2InfoContent.h"
#include "NrppaInformation.h"
#include "N2InformationClass.h"
#include "N2InfoContainer.h"
#include "N1N2MessageTransferReqData.h"

#include "InitiatingMessage.h"
#include "ProtocolIE-Field.h"

using namespace std::string_literals;
using namespace oai::lmf_server;

void LMFContext::finish() {
  model::LocationData locationData;

  promise.set_value(locationData);
}

bool LMFContext::n1_n2_transfer(
    NRPPA_PDU_t* nrppaPdu, nlohmann::json& json_data,
    Pistache::Http::Code& code) {
  xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppaPdu);

  asn_encode_to_new_buffer_result_t nrppaPduEnc = asn_encode_to_new_buffer(
      0, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_NRPPA_PDU, nrppaPdu);

  if (nrppaPduEnc.result.encoded == -1) {
    Logger::lmf_app().error(
        "Could not encode (at %s)\n", nrppaPduEnc.result.failed_type ?
                                          nrppaPduEnc.result.failed_type->name :
                                          "unknown");
    model::ProblemDetails problemDetails;
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
    return false;
  }
  Logger::lmf_server().debug("<<- vor asn_decode ->>");

  std::string amf_uri  = {};
  std::string method   = "POST";
  std::string response = {};
  amf_uri =
      "http://" +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.amf_addr.ipv4_addr))) +
      ":" + std::to_string(lmf_cfg.amf_addr.port) + "/namf-comm/" +
      lmf_cfg.amf_addr.api_version + "/ue-contexts/" + this->supi +
      "/n1-n2-messages";
  Logger::lmf_app().debug("AMF's URI %s", amf_uri.c_str());

  std::string nrppaMsgStr(
      (char*) nrppaPduEnc.buffer, nrppaPduEnc.result.encoded);
  std::string nrppaMsgHex = {};
  conv::convert_string_2_hex(nrppaMsgStr, nrppaMsgHex);
  free(nrppaPduEnc.buffer);

  model::RefToBinaryData ngapData = {};
  ngapData.setContentId(N2_NRPPa_CONTENT_ID);

  model::NgapIeType ngapIeType = {};
  ngapIeType.setEnumValue(
      model::NgapIeType_anyOf::eNgapIeType_anyOf::NRPPA_PDU);

  model::N2InfoContent n2InfoContent = {};
  n2InfoContent.setNgapIeType(ngapIeType);
  n2InfoContent.setNgapData(ngapData);

  model::NrppaInformation nrppaInformation = {};
  nrppaInformation.setNfId(lmf_nrf_inst->lmf_nf_profile.get_nf_instance_id());
  nrppaInformation.setNrppaPdu(n2InfoContent);

  model::N2InformationClass n2InformationClass = {};
  n2InformationClass.setEnumValue(
      model::N2InformationClass_anyOf::eN2InformationClass_anyOf::NRPPA);
  model::N2InfoContainer n2InfoContainer = {};
  n2InfoContainer.setN2InformationClass(n2InformationClass);
  n2InfoContainer.setNrppaInfo(nrppaInformation);

  model::N1N2MessageTransferReqData n1n2MessageTransferReqData = {};
  n1n2MessageTransferReqData.setN2InfoContainer(n2InfoContainer);

  nlohmann::json n1n2MessageTransferReq_json;
  to_json(n1n2MessageTransferReq_json, n1n2MessageTransferReqData);

  std::string body      = {};
  std::string json_part = {};
  json_part             = n1n2MessageTransferReq_json.dump();

  mime_parser::create_multipart_related_content(
      body, json_part, CURL_MIME_BOUNDARY, nrppaMsgHex,
      multipart_related_content_part_e::NGAP);

  lmf_client_inst->curl_http_client(amf_uri, method, body, response, true);
  Logger::lmf_app().info("Response from AMF: %s", response);

  auto const& rspData_json = nlohmann::json::parse(response);
  if (!rspData_json.contains("cause") ||
      rspData_json["cause"] !=
          n1_n2_message_transfer_cause_e2str[N1_N2_TRANSFER_INITIATED]) {
    auto const& cause =
        rspData_json.contains("cause") ?
            n1_n2_message_transfer_cause_e2str[rspData_json["cause"]] :
            "no cause"s;
    auto const& err = "n1n2message transfer failed supi: '"s + this->supi +
                      "': cause: "s + cause;
    Logger::lmf_app().warn(err);
    model::ProblemDetails problemDetails;
    problemDetails.setCause("INTERNAL_SERVER_ERROR");
    problemDetails.setStatus(HTTP_RESPONSE_CODE_INTERNAL_SERVER_ERROR);
    problemDetails.setDetail(err);

    json_data = problemDetails;
    code      = Pistache::Http::Code(problemDetails.getStatus());

    return false;
  }
  return true;
}

void LMFContext::determine_location(
    const model::InputData& inputData, nlohmann::json& json_data,
    Pistache::Http::Code& code) {
  Logger::lmf_app().info("Handle Determin Location Request");

  auto initiatingMessage = InitiatingMessage_t{
      .procedureCode      = ProcedureCode_id_positioningInformationExchange,
      .criticality        = Criticality_reject,
      .nrppatransactionID = 10,
      .value =
          {.present =
               InitiatingMessage__value_PR_PositioningInformationRequest},
  };
  auto ies = &initiatingMessage.value.choice.PositioningInformationRequest
                  .protocolIEs.list;

  auto requestedSRSTransmissionCharacteristics = PositioningInformationRequest_IEs_t{
      .id          = ProtocolIE_ID_id_RequestedSRSTransmissionCharacteristics,
      .criticality = Criticality_ignore,
      .value =
          {
              .present =
                  PositioningInformationRequest_IEs__value_PR_RequestedSRSTransmissionCharacteristics,
              .choice =
                  {
                      .RequestedSRSTransmissionCharacteristics =
                          {
                              .resourceType =
                                  RequestedSRSTransmissionCharacteristics__resourceType_aperiodic,
                              .bandwidth =
                                  {
                                      .present = BandwidthSRS_PR_fR1,
                                      .choice = {.fR1 = BandwidthSRS__fR1_mHz5},
                                  },
                          },
                  },
          },
  };
  ASN_SEQUENCE_ADD(ies, &requestedSRSTransmissionCharacteristics);

  auto nrppaPdu = NRPPA_PDU_t{
      .present = NRPPA_PDU_PR_initiatingMessage,
      .choice  = {.initiatingMessage = &initiatingMessage},
  };

  this->n1_n2_transfer(&nrppaPdu, json_data, code);
}

/*
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
*/

/*
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
  n1MessageContainer.setN1MessageContent(n1MessageData);
*/
/*
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
*/