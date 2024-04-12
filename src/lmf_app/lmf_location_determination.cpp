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

#include "lmf_location_determination.hpp"

#include "lmf.h"
#include "lmf_app.hpp"
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
#include "N2InformationTransferReqData.h"

#include "InitiatingMessage.h"
#include "ProtocolIE-Field.h"
#include "SemipersistentSRS.h"
#include "AperiodicSRS.h"
#include "TRP-MeasurementRequestItem.h"

using namespace std::string_literals;
using namespace oai::lmf_server;

// provides for asn container.list.array range based for loops
// for (auto const& xyzIEs : xyzResponse.protocolIEs) {
template<typename T>
auto begin(T const& container) {
  return container.list.array;
}

template<typename T>
auto end(T const& container) {
  return container.list.array + container.list.count;
}

bool LocationDetermination::n1_n2_message_transfer(
    NRPPA_PDU_t* nrppaPdu, NRPPATransactionID_t const& txnId,
    ProcedureCode_t const& procedureCode) {
  Logger::lmf_app().info("n1_n2_message_transfer");
  // xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppaPdu);

  asn_encode_to_new_buffer_result_t nrppaPduEnc = asn_encode_to_new_buffer(
      0, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_NRPPA_PDU, nrppaPdu);
  ASN_STRUCT_FREE(asn_DEF_NRPPA_PDU, nrppaPdu);

  if (nrppaPduEnc.result.encoded == -1) {
    Logger::lmf_app().error(
        "Could not encode (at %s)\n", nrppaPduEnc.result.failed_type ?
                                          nrppaPduEnc.result.failed_type->name :
                                          "unknown");
    auto const& title  = "asn nrppa encode failed"s;
    auto const& field  = nrppaPduEnc.result.failed_type ?
                             nrppaPduEnc.result.failed_type->name :
                             "unknown";
    auto const& detail = "Could not encode at; "s + field;
    throwHttpError(title, detail);
  }
  // gc free(nrppaPduEnc.buffer)
  std::unique_ptr<void, decltype(&std::free)> gc{
      nrppaPduEnc.buffer, &std::free};

  std::string amf_uri  = {};
  std::string method   = "POST";
  std::string response = {};
  amf_uri =
      "http://" +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.amf_addr.ipv4_addr))) +
      ":" + std::to_string(lmf_cfg.amf_addr.port) + NAMF_BASE +
      lmf_cfg.amf_addr.api_version + NAMF_N1N2_SUBSCRIBE_BASE + this->supi +
      NAMF_N1N2_SUBSCRIBE_MESSAGES;
  Logger::lmf_app().debug("AMF's URI %s", amf_uri.c_str());

  std::string nrppaMsgStr(
      (char*) nrppaPduEnc.buffer, nrppaPduEnc.result.encoded);
  std::string nrppaMsgHex = {};
  conv::convert_string_2_hex(nrppaMsgStr, nrppaMsgHex);

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
    auto const& title = "n1n2message transfer failed"s;
    auto const& cause =
        rspData_json.contains("cause") ?
            n1_n2_message_transfer_cause_e2str[rspData_json["cause"]] :
            "no cause"s;
    auto const& detail = "supi: '"s + this->supi + "': cause: "s + cause;
    throwHttpError(title, detail);
  }

  if (auto const& [iter, inserted] =
          this->nrppa_tId.try_emplace(txnId, procedureCode);
      !inserted) {
    throwHttpError(
        "n1_n2_message_transfer"s,
        "nrppa id "s + std::to_string(txnId) + " reuse"s);
  }

  return true;
}

bool LocationDetermination::non_ue_n2_message_transfer(
    NRPPA_PDU_t* nrppaPdu, NRPPATransactionID_t const& txnId,
    ProcedureCode_t const& procedureCode,
    std::vector<model::GlobalRanNodeId> const& globalRanNodeList,
    SRSConfiguration_t* const ueSrsConfigurationShared) {
  Logger::lmf_app().info("non_ue_n2_message_transfer");
  // xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppaPdu);

  asn_encode_to_new_buffer_result_t nrppaPduEnc = asn_encode_to_new_buffer(
      0, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_NRPPA_PDU, nrppaPdu);
  if (ueSrsConfigurationShared != nullptr) {
    // don't free, it's from positioning information request
    *ueSrsConfigurationShared = {};
  }
  ASN_STRUCT_FREE(asn_DEF_NRPPA_PDU, nrppaPdu);

  if (nrppaPduEnc.result.encoded == -1) {
    Logger::lmf_app().error(
        "Could not encode (at %s)\n", nrppaPduEnc.result.failed_type ?
                                          nrppaPduEnc.result.failed_type->name :
                                          "unknown");
    auto const& title  = "asn nrppa encode failed"s;
    auto const& field  = nrppaPduEnc.result.failed_type ?
                             nrppaPduEnc.result.failed_type->name :
                             "unknown";
    auto const& detail = "Could not encode at; "s + field;
    throwHttpError(title, detail);
  }

  std::string amf_uri  = {};
  std::string method   = "POST";
  std::string response = {};
  amf_uri =
      "http://" +
      std::string(inet_ntoa(*((struct in_addr*) &lmf_cfg.amf_addr.ipv4_addr))) +
      ":" + std::to_string(lmf_cfg.amf_addr.port) + NAMF_BASE +
      lmf_cfg.amf_addr.api_version + NAMF_NON_UE_N2_MESSAGE_TRANSFER;
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

  model::N2InformationTransferReqData n2InformationTransferReqData;
  n2InformationTransferReqData.setN2Information(n2InfoContainer);
  if (globalRanNodeList.size() > 0) {
    Logger::lmf_app().debug(
        "non_ue_n2_message_transfer: globalRanNodeList set, send to %d gNBs",
        globalRanNodeList.size());
    n2InformationTransferReqData.setGlobalRanNodeList(globalRanNodeList);
  } else {
    Logger::lmf_app().debug(
        "non_ue_n2_message_transfer: globalRanNodeList not set, send to all "
        "gNBs using ratSelector");
    n2InformationTransferReqData.setRatSelector("NR");
  }

  nlohmann::json n2InformationTransferReqData_json;
  to_json(n2InformationTransferReqData_json, n2InformationTransferReqData);

  std::string body      = {};
  std::string json_part = {};
  json_part             = n2InformationTransferReqData_json.dump();

  mime_parser::create_multipart_related_content(
      body, json_part, CURL_MIME_BOUNDARY, nrppaMsgHex,
      multipart_related_content_part_e::NGAP);

  lmf_client_inst->curl_http_client(amf_uri, method, body, response, true);
  Logger::lmf_app().info("Response from AMF: %s", response);

  // model::N2InformationTransferRspData;
  // model::N2InformationTransferError
  // model::N2InformationTransferResult

  auto const& rspData_json = nlohmann::json::parse(response);
  if (!rspData_json.contains("cause") ||
      rspData_json["cause"] != non_ue_n2_message_transfer_cause_e2str
                                   [NON_UE_N2_TRANSFER_INITIATED]) {
    auto const& title = "non-ue-n2-message transfer failed"s;
    auto const& cause =
        rspData_json.contains("cause") ?
            non_ue_n2_message_transfer_cause_e2str[rspData_json["cause"]] :
            "no cause"s;
    auto const& detail = "supi: '"s + this->supi + "': cause: "s + cause;
    throwHttpError(title, detail);
  }

  if (auto const& [iter, inserted] =
          this->nrppa_tId.try_emplace(txnId, procedureCode);
      !inserted) {
    throwHttpError(
        "non-ue n2 message transfer: "s,
        "nrppa id "s + std::to_string(txnId) + " reuse"s);
  }
  lmf_app_inst->insert_nrppaTxnId2supi(txnId, this->supi);

  return true;
}

template<typename T>
T LocationDetermination::positioning_wait_for(
    std::string const& kind, NRPPATransactionID_t const& tId,
    std::promise<T>& p) {
  auto const& wait_ms = lmf_cfg.positioning_wait_ms.count();
  Logger::lmf_app().info(
      "waiting %dms for positioning %s response for supi %s, tId: %d", wait_ms,
      kind, this->supi, tId);

  auto f = p.get_future();
  switch (auto const& rc = f.wait_for(lmf_cfg.positioning_wait_ms); rc) {
    case std::future_status::timeout: {
      this->throwHttpError(
          "positioning "s + kind + " request timeout"s,
          "waited "s + std::to_string(lmf_cfg.positioning_wait_ms.count()) +
              "ms"s);
    } break;

    case std::future_status::ready: {
      Logger::lmf_app().info(
          "positioning "s + kind + " received for supi: %s"s, this->supi);
    } break;

    default:
      this->throwHttpError(
          "positioning "s + kind,
          "unhandled future_status: "s + std::to_string(static_cast<int>(rc)));
  }
  return f.get();
}

std::tuple<
    NRPPA_PDU_t*, PositioningInformationResponse_t const&,
    SRSConfiguration_t const&>
LocationDetermination::positioning_information_request() {
  auto const& tId = lmf_app_inst->nrppa_tid_gen.get_uid();

  auto initiatingMessage =
      (InitiatingMessage_t*) malloc(sizeof(InitiatingMessage_t));
  *initiatingMessage = InitiatingMessage_t{
      .procedureCode      = ProcedureCode_id_positioningInformationExchange,
      .criticality        = Criticality_reject,
      .nrppatransactionID = tId,
      .value =
          {.present =
               InitiatingMessage__value_PR_PositioningInformationRequest},
  };
  auto ies = &initiatingMessage->value.choice.PositioningInformationRequest
                  .protocolIEs.list;

  auto requestedSRSTransmissionCharacteristics =
      (PositioningInformationRequest_IEs_t*) malloc(
          sizeof(PositioningInformationRequest_IEs_t));
  *requestedSRSTransmissionCharacteristics = PositioningInformationRequest_IEs_t{
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
                                      .choice =
                                          {.fR1 = BandwidthSRS__fR1_kHz100},
                                  },
                          },
                  },
          },
  };
  ASN_SEQUENCE_ADD(ies, requestedSRSTransmissionCharacteristics);

  auto nrppaPdu = (NRPPA_PDU_t*) malloc(sizeof(NRPPA_PDU_t));
  *nrppaPdu     = NRPPA_PDU_t{
      .present = NRPPA_PDU_PR_initiatingMessage,
      .choice  = {.initiatingMessage = initiatingMessage},
  };

  this->positioning_information_response = {};
  this->n1_n2_message_transfer(
      nrppaPdu, tId, ProcedureCode_id_positioningInformationExchange);
  return this->positioning_wait_for(
      "information", tId, this->positioning_information_response);
}

std::future<std::pair<NRPPA_PDU_t*, MeasurementResponse_t const&>>
LocationDetermination::measurement_request(
    NRPPATransactionID_t const& tId, Measurement_ID_t const& mId,
    std::vector<model::GlobalRanNodeId> const& globalRanNodeList,
    std::set<TRP_ID_t> const& trpIds,
    SRSConfiguration_t const& srsConfigurationUE) {
  Logger::lmf_app().info("measurement request");
  auto initiatingMessage =
      (InitiatingMessage_t*) malloc(sizeof(InitiatingMessage_t));
  *initiatingMessage = InitiatingMessage_t{
      .procedureCode      = ProcedureCode_id_Measurement,
      .criticality        = Criticality_reject,
      .nrppatransactionID = tId,
      .value = {.present = InitiatingMessage__value_PR_MeasurementRequest},
  };
  auto ies =
      &initiatingMessage->value.choice.MeasurementRequest.protocolIEs.list;

  auto lmfMeasurementIdIe =
      (MeasurementRequest_IEs_t*) malloc(sizeof(MeasurementRequest_IEs_t));
  *lmfMeasurementIdIe = MeasurementRequest_IEs_t{
      .id          = ProtocolIE_ID_id_LMF_Measurement_ID,
      .criticality = Criticality_reject,
      .value =
          {
              .present = MeasurementRequest_IEs__value_PR_Measurement_ID,
              .choice  = {.Measurement_ID = mId},
          },
  };
  ASN_SEQUENCE_ADD(ies, lmfMeasurementIdIe);

  auto trpMeasurementRequestListIe =
      (MeasurementRequest_IEs_t*) malloc(sizeof(MeasurementRequest_IEs_t));
  *trpMeasurementRequestListIe = {
      .id          = ProtocolIE_ID_id_TRP_MeasurementRequestList,
      .criticality = Criticality_reject,
      .value =
          {
              .present =
                  MeasurementRequest_IEs__value_PR_TRP_MeasurementRequestList,
          },
  };
  auto trpMeasurementRequestList = &trpMeasurementRequestListIe->value.choice
                                        .TRP_MeasurementRequestList.list;
  for (auto const& trpId : trpIds) {
    auto trpMeasurementRequestItem = (TRP_MeasurementRequestItem_t*) malloc(
        sizeof(TRP_MeasurementRequestItem_t));
    *trpMeasurementRequestItem = TRP_MeasurementRequestItem_t{
        .tRP_ID = trpId,
    };
    ASN_SEQUENCE_ADD(trpMeasurementRequestList, trpMeasurementRequestItem);
  }
  ASN_SEQUENCE_ADD(ies, trpMeasurementRequestListIe);

  auto reportCharacteristics =
      (MeasurementRequest_IEs_t*) malloc(sizeof(MeasurementRequest_IEs_t));
  *reportCharacteristics = MeasurementRequest_IEs_t{
      .id          = ProtocolIE_ID_id_ReportCharacteristics,
      .criticality = Criticality_reject,
      .value =
          {
              .present = MeasurementRequest_IEs__value_PR_ReportCharacteristics,
              .choice =
                  {
                      .ReportCharacteristics = ReportCharacteristics_onDemand,
                  },
          },
  };
  ASN_SEQUENCE_ADD(ies, reportCharacteristics);

  auto srsConfigurationIE =
      (MeasurementRequest_IEs_t*) malloc(sizeof(MeasurementRequest_IEs_t));
  *srsConfigurationIE = MeasurementRequest_IEs_t{
      .id          = ProtocolIE_ID_id_SRSConfiguration,
      .criticality = Criticality_ignore,
      .value =
          {
              .present = MeasurementRequest_IEs__value_PR_SRSConfiguration,
              .choice =
                  {
                      .SRSConfiguration = srsConfigurationUE,
                  },
          },
  };
  auto ueSrsConfigurationShared =
      &srsConfigurationIE->value.choice.SRSConfiguration;
  ASN_SEQUENCE_ADD(ies, srsConfigurationIE);

  auto nrppaPdu = (NRPPA_PDU_t*) malloc(sizeof(NRPPA_PDU_t));
  *nrppaPdu     = NRPPA_PDU_t{
      .present = NRPPA_PDU_PR_initiatingMessage,
      .choice  = {.initiatingMessage = initiatingMessage},
  };

  this->measurement_response = {};  // clear promise
  this->non_ue_n2_message_transfer(
      nrppaPdu, tId, ProcedureCode_id_Measurement, globalRanNodeList,
      ueSrsConfigurationShared);
  return this->measurement_response.get_future();
}

void LocationDetermination::handle_positioning_information_response(
    NRPPA_PDU_t* nrppaPdu, NRPPATransactionID_t const& tId,
    PositioningInformationResponse_t const& positioningInformationResponse) {
  Logger::lmf_app().info("handle positioning information response");
  SRSConfiguration_t* srsConfiguration = nullptr;
  for (auto const& positioningInformationIE :
       positioningInformationResponse.protocolIEs) {
    if (positioningInformationIE->id == ProtocolIE_ID_id_SRSConfiguration &&
        positioningInformationIE->value.present ==
            PositioningInformationResponse_IEs__value_PR_SRSConfiguration) {
      srsConfiguration =
          &positioningInformationIE->value.choice.SRSConfiguration;
    }
  }
  if (srsConfiguration == nullptr) {
    try {
      throwHttpError(
          "handle_positioning_information_response: srsConfiguration missing",
          "srsConfiguration needed for non-ue measurement request");
    } catch (...) {
      this->positioning_information_response.set_exception(
          std::current_exception());
      throw;
    }
  }
  this->positioning_information_response.set_value(
      {nrppaPdu, positioningInformationResponse, *srsConfiguration});
}

void LocationDetermination::handle_positioning_information_failure(
    NRPPA_PDU_t* nrppaPdu,
    PositioningInformationFailure_t const& positioningInformationFailure) {
  for (auto const& positioningInformationFailureIe :
       positioningInformationFailure.protocolIEs) {
    switch (positioningInformationFailureIe->id) {
      case ProtocolIE_ID_id_Cause: {
      } break;

      case ProtocolIE_ID_id_CriticalityDiagnostics: {
      } break;

      default:
        Logger::lmf_app().warn(
            "positioningInformationFailure: unknwon IE id: %d",
            positioningInformationFailureIe->id);
    }
  }
}

void LocationDetermination::handle_measurement_response(
    NRPPA_PDU_t* nrppaPdu, NRPPATransactionID_t const& tId,
    MeasurementResponse_t const& measurementResponse) {
  Logger::lmf_app().info("handle measurement response");
  for (auto const& ie : measurementResponse.protocolIEs) {
    if (ie->id == ProtocolIE_ID_id_LMF_Measurement_ID &&
        ie->value.present == MeasurementResponse_IEs__value_PR_Measurement_ID) {
      auto const& measurementID = ie->value.choice.Measurement_ID;
      this->measurement_response.set_value({nrppaPdu, measurementResponse});
    }
  }
}

// 9.1.1.17 POSITIONING ACTIVATION REQUEST
std::pair<NRPPA_PDU_t*, PositioningActivationResponse_t const&>
LocationDetermination::positioning_activation_request() {
  auto const& tId = lmf_app_inst->nrppa_tid_gen.get_uid();

  auto initiatingMessage =
      (InitiatingMessage_t*) malloc(sizeof(InitiatingMessage_t));
  *initiatingMessage = InitiatingMessage_t{
      .procedureCode      = ProcedureCode_id_positioningActivation,
      .criticality        = Criticality_reject,
      .nrppatransactionID = tId,
      .value =
          {.present = InitiatingMessage__value_PR_PositioningActivationRequest},
  };
  auto ies = &initiatingMessage->value.choice.PositioningActivationRequest
                  .protocolIEs.list;

  // >Aperiodic
  auto aperiodicSRS = (AperiodicSRS_t*) malloc(sizeof(AperiodicSRS_t));
  *aperiodicSRS     = AperiodicSRS_t{
      .aperiodic = AperiodicSRS__aperiodic_true,
  };
  // CHOICE SRS type
  auto aperiodicSRS_ie = (PositioningActivationRequestIEs_t*) calloc(
      1, sizeof(PositioningActivationRequestIEs_t));
  *aperiodicSRS_ie = PositioningActivationRequestIEs_t{
      .id          = ProtocolIE_ID_id_SRSType,
      .criticality = Criticality_reject,
      .value =
          {
              .present = PositioningActivationRequestIEs__value_PR_SRSType,
              .choice =
                  {
                      .SRSType =
                          {
                              .present = SRSType_PR_aperiodicSRS,
                              .choice =
                                  {
                                      .aperiodicSRS = aperiodicSRS,
                                  },
                          },
                  },
          },
  };
  ASN_SEQUENCE_ADD(ies, aperiodicSRS_ie);
#if 0
  // >Semi-persistent
  auto semipersistentSRS =
      (SemipersistentSRS_t*) malloc(sizeof(SemipersistentSRS_t));
  *semipersistentSRS = SemipersistentSRS_t{
      .sRSResourceSetID = 1,
  };
  // CHOICE SRS type
  auto semipersistentSRS_ie = (PositioningActivationRequestIEs_t*) malloc(
      sizeof(PositioningActivationRequestIEs_t));
  *semipersistentSRS_ie = PositioningActivationRequestIEs_t{
      .id          = ProtocolIE_ID_id_SRSType,
      .criticality = Criticality_reject,
      .value =
          {
              .present = PositioningActivationRequestIEs__value_PR_SRSType,
              .choice =
                  {
                      .SRSType =
                          {
                              .present = SRSType_PR_semipersistentSRS,
                              .choice =
                                  {
                                      .semipersistentSRS = semipersistentSRS,
                                  },
                          },
                  },
          },
  };
  ASN_SEQUENCE_ADD(ies, semipersistentSRS_ie);
#endif
  auto nrppaPdu = (NRPPA_PDU_t*) malloc(sizeof(NRPPA_PDU_t));
  *nrppaPdu     = NRPPA_PDU_t{
      .present = NRPPA_PDU_PR_initiatingMessage,
      .choice  = {.initiatingMessage = initiatingMessage},
  };
  this->positioning_activation_response = {};
  this->n1_n2_message_transfer(
      nrppaPdu, tId, ProcedureCode_id_positioningActivation);
  return this->positioning_wait_for(
      "activation", tId, this->positioning_activation_response);
}

void LocationDetermination::handle_positioning_activation_response(
    NRPPA_PDU_t* nrppaPdu, NRPPATransactionID_t const& tId,
    PositioningActivationResponse_t const& positioningActivationResponse) {
  Logger::lmf_app().info("handle positioning activation response");
  this->positioning_activation_response.set_value(
      {nrppaPdu, positioningActivationResponse});
}

void LocationDetermination::throwHttpError(
    std::string const& title, std::string const& detail,
    Pistache::Http::Code const& code) {
  oai::lmf::app::throwHttpError(title, detail, this->supi, code);
}