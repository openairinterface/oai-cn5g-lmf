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

#include <unistd.h>
#include <iostream>
#include <iterator>
#include <string>

#include "lmf_app.hpp"
#include "lmf_nrf.hpp"
#include "lmf_client.hpp"

#include "logger.hpp"
#include "conversions.hpp"
#include "mime_parser.hpp"
#include "3gpp_29.518.h"

#include "LocationData.h"
#include "N1MessageContainer.h"
#include "N1MessageClass.h"
#include "N1N2MessageTransferReqData.h"
#include "N1N2MessageTransferRspData.h"
#include "N2InformationTransferReqData.h"
#include "UeN1N2InfoSubscriptionCreateData.h"
#include "UeN1N2InfoSubscriptionCreatedData.h"
#include "RefToBinaryData.h"
#include "ProblemDetails.h"

#include "InitiatingMessage.h"
#include "ProtocolIE-Field.h"

using namespace std;
using namespace oai::lmf::app;
using namespace oai::lmf_server::model;
using namespace config;

lmf_client* lmf_client_inst = nullptr;
lmf_nrf* lmf_nrf_inst       = nullptr;

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
#if 0
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
#endif
  }
  Logger::lmf_app().startup("Started");
}

//------------------------------------------------------------------------------
lmf_app::~lmf_app() {
  Logger::lmf_app().debug("Delete LMF_APP instance...");
}

void lmf_app::handle_determine_location(
    const InputData& inputData, nlohmann::json& json_data,
    Pistache::Http::Code& code) {
  auto const& supi = inputData.getSupi();
  auto const& ctx  = create_lmf_context(supi);
  if (!ctx) {
    auto const& err =
        "Could not create context for supi '"s + supi + "': already exist"s;
    Logger::lmf_app().warn(err);
    ProblemDetails problemDetails;
    problemDetails.setCause("INTERNAL_SERVER_ERROR");
    problemDetails.setStatus(HTTP_RESPONSE_CODE_INTERNAL_SERVER_ERROR);
    problemDetails.setDetail(err);

    json_data = problemDetails;
    code      = Pistache::Http::Code(problemDetails.getStatus());

    return;
  }
  auto const& subs = create_n1n2subscription(supi);
  if (!subs) {
    auto const& err = "Could not subscribe for n1n2message '"s + supi;
    Logger::lmf_app().warn(err);
    ProblemDetails problemDetails;
    problemDetails.setCause("INTERNAL_SERVER_ERROR");
    problemDetails.setStatus(HTTP_RESPONSE_CODE_INTERNAL_SERVER_ERROR);
    problemDetails.setDetail(err);

    json_data = problemDetails;
    code      = Pistache::Http::Code(problemDetails.getStatus());

    return;
  }
  ctx->determine_location(inputData, json_data, code);
  json_data = ctx->promise.get_future().get();
  // stay subscribed
  // release_n1n2subscription(supi);
  code = Pistache::Http::Code::Ok;

  auto initiatingMessage = InitiatingMessage_t{
      .procedureCode      = ProcedureCode_id_Measurement,
      .criticality        = Criticality_reject,
      .nrppatransactionID = 11,
      .value = {.present = InitiatingMessage__value_PR_MeasurementRequest},
  };
  auto ies =
      &initiatingMessage.value.choice.MeasurementRequest.protocolIEs.list;

  auto lmfMeasurementId = MeasurementRequest_IEs_t{
      .id          = ProtocolIE_ID_id_LMF_Measurement_ID,
      .criticality = Criticality_reject,
      .value =
          {
              .present = MeasurementRequest_IEs__value_PR_Measurement_ID,
              .choice  = {.Measurement_ID = 1},
          },
  };
  ASN_SEQUENCE_ADD(ies, &lmfMeasurementId);

  auto reportCharacteristics = MeasurementRequest_IEs_t{
      .id          = ProtocolIE_ID_id_ReportCharacteristics,
      .criticality = Criticality_reject,
      .value =
          {
              .present = MeasurementRequest_IEs__value_PR_ReportCharacteristics,
              .choice =
                  {.ReportCharacteristics = ReportCharacteristics_onDemand},
          },
  };
  ASN_SEQUENCE_ADD(ies, &reportCharacteristics);

  auto nrppaPdu = NRPPA_PDU_t{
      .present = NRPPA_PDU_PR_initiatingMessage,
      .choice  = {.initiatingMessage = &initiatingMessage},
  };

  ctx->promise = {};
  ctx->n1_n2_transfer(&nrppaPdu, json_data, code);
  json_data = ctx->promise.get_future().get();

  del_supi_2_context(supi);
}

bool lmf_app::_is_supi_2_context(const std::string& supi) const {
  return (supi2ctx.count(supi) > 0) && (supi2ctx.at(supi) != nullptr);
}

bool lmf_app::is_supi_2_context(const string& supi) const {
  std::shared_lock lock(m_supi2ctx);
  return _is_supi_2_context(supi);
}

std::shared_ptr<LMFContext> lmf_app::create_lmf_context(const string& supi) {
  std::unique_lock lock(m_supi2ctx);

  if (_is_supi_2_context(supi)) {
    return {nullptr};
  }
  return supi2ctx[supi] = std::make_shared<LMFContext>(supi);
}

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

std::shared_ptr<N1N2MessageSubscription>
oai::lmf::app::lmf_app::create_n1n2subscription(const std::string& supi) {
  std::unique_lock lock(m_supi2n1n2subs);

  if (supi2n1n2subs.count(supi) > 0 && supi2n1n2subs.at(supi) != nullptr) {
    auto subscription = supi2n1n2subs.at(supi);
    Logger::lmf_app().info(
        "n1n2info subscription already subscribed for supi: %s id: %s"s,
        subscription->supi, subscription->id);
    return subscription;
  }

  auto subscription = N1N2MessageSubscription::create(supi);

  if (subscription->is_subscribed()) {
    Logger::lmf_app().info(
        "n1n2info subscription created for supi: %s id: %s"s,
        subscription->supi, subscription->id);
    return supi2n1n2subs[supi] = subscription;
  }

  return {nullptr};
}

void oai::lmf::app::lmf_app::release_n1n2subscription(const std::string& supi) {
  std::unique_lock lock(m_supi2n1n2subs);

  supi2n1n2subs.erase(supi);
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

  return true;
}
#if 0
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
#endif