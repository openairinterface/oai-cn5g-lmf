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
#include "SuccessfulOutcome.h"
#include "UnsuccessfulOutcome.h"
#include "ProtocolIE-Field.h"
#include "TRPItem.h"  // not included in TRPList.h for wre

using namespace std;
using namespace oai::lmf::app;
using namespace oai::lmf_server::model;
using namespace config;

lmf_client* lmf_client_inst = nullptr;
lmf_nrf* lmf_nrf_inst       = nullptr;

void oai::lmf::app::throwHttpError(
    std::string const& title, std::string const& detail,
    Pistache::Http::Code const& code) {
  using namespace Pistache::Http;
  oai::lmf_server::model::ProblemDetails problemDetails;
  problemDetails.setTitle(title);
  problemDetails.setDetail(detail);
  Logger::lmf_server().error(
      problemDetails.getTitle() + ": " + problemDetails.getDetail());
  auto const& reason = nlohmann::json(problemDetails).dump();
  throw HttpError{code, reason};
}

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
    this->nonUeN2MessageSubscription = NonUeN2MessageSubscription::create();
    auto [nrppaPduEnc, gcBuf] = build_trp_information_request_nrppa_pdu();

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
  this->create_n1n2subscription(supi);
  ctx->position_information_request(inputData, json_data, code);
  json_data = ctx->promise.get_future().get();
  // stay subscribed
  // release_n1n2subscription(supi);
  code = Pistache::Http::Code::Ok;

  auto measurementID        = Measurement_ID_t{1};
  auto reportCharacteristic = ReportCharacteristics_onDemand;

  auto lmfMeasurementId = MeasurementRequest_IEs_t{
      .id          = ProtocolIE_ID_id_LMF_Measurement_ID,
      .criticality = Criticality_reject,
      .value =
          {
              .present = MeasurementRequest_IEs__value_PR_Measurement_ID,
              .choice  = {.Measurement_ID = measurementID},
          },
  };

  auto reportCharacteristics = MeasurementRequest_IEs_t{
      .id          = ProtocolIE_ID_id_ReportCharacteristics,
      .criticality = Criticality_reject,
      .value =
          {
              .present = MeasurementRequest_IEs__value_PR_ReportCharacteristics,
              .choice =
                  {.ReportCharacteristics =
                       ReportCharacteristics_t{reportCharacteristic}},
          },
  };

  auto initiatingMessage = InitiatingMessage_t{
      .procedureCode      = ProcedureCode_id_Measurement,
      .criticality        = Criticality_reject,
      .nrppatransactionID = 11,
      .value = {.present = InitiatingMessage__value_PR_MeasurementRequest},
  };
  auto ies =
      &initiatingMessage.value.choice.MeasurementRequest.protocolIEs.list;
  ASN_SEQUENCE_ADD(ies, &lmfMeasurementId);
  ASN_SEQUENCE_ADD(ies, &reportCharacteristics);

  auto nrppaPdu = NRPPA_PDU_t{
      .present = NRPPA_PDU_PR_initiatingMessage,
      .choice  = {.initiatingMessage = &initiatingMessage},
  };

  ctx->promise = {};
  ctx->n1_n2_message_transfer(&nrppaPdu, json_data, code);
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

std::shared_ptr<LocationDetermination> lmf_app::create_lmf_context(
    const string& supi) {
  std::unique_lock lock(m_supi2ctx);

  if (_is_supi_2_context(supi)) {
    return {nullptr};
  }
  return supi2ctx[supi] = std::make_shared<LocationDetermination>(supi);
}

std::shared_ptr<LocationDetermination> lmf_app::supi_2_context(
    const std::string& supi) const {
  std::shared_lock lock(m_supi2ctx);

  if (!_is_supi_2_context(supi)) {
    return {nullptr};
  }
  return supi2ctx.at(supi);
}

void lmf_app::set_supi_2_context(
    const string& supi, const std::shared_ptr<LocationDetermination>& lc) {
  std::unique_lock lock(m_supi2ctx);
  supi2ctx[supi] = lc;
}

void lmf_app::del_supi_2_context(const string& supi) {
  std::unique_lock lock(m_supi2ctx);
  supi2ctx.erase(supi);
}

void lmf_app::create_n1n2subscription(const std::string& supi) {
  std::unique_lock lock(m_supi2n1n2subs);

  auto const& [iter, inserted] = supi2n1n2subs.try_emplace(supi, supi);
  auto const& subscription     = iter->second;
  Logger::lmf_app().info(
      "n1n2info %s for supi: %s id: %s"s,
      inserted ? "subscription created"s : "already subscribed"s,
      subscription.supi, subscription.id);
}

void oai::lmf::app::lmf_app::release_n1n2subscription(const std::string& supi) {
  std::unique_lock lock(m_supi2n1n2subs);

  supi2n1n2subs.erase(supi);
}

bool lmf_app::handle_non_ue_n2info_nrppa_notification(
    NRPPA_PDU_t* nrppa, ProblemDetails& problem_details, uint8_t& http_code) {
  if (nrppa->present != NRPPA_PDU_PR_successfulOutcome) {
    Logger::lmf_server().error(
        "nrppa->present != NRPPA_PDU_PR_successfulOutcome: %d", nrppa->present);
    return false;
  }

  return true;
}

// check 1:1 relationship between procedureCode and value.present
template<typename T, typename U>
static void checkPresent(T const& present, U const& expected) {
  if (present != expected) {
    auto const &title  = "handle_n2info_nrppa_notification: invalid message"s,
               &ps     = "present: "s + std::to_string(present),
               &es     = "expected: "s + std::to_string(expected),
               &detail = ps + ": "s + es;
    throwHttpError(title, detail);
  }
}

bool lmf_app::handle_n2info_nrppa_notification(
    std::string supi, NRPPA_PDU_t* nrppa) {
  auto ctx = this->supi_2_context(supi);
  if (!ctx) {
    Logger::lmf_server().error("N2InfoNotify: unknown supi: %s", supi);
    return false;
  }

  switch (nrppa->present) {
    case NRPPA_PDU_PR_initiatingMessage: {
      auto const& initiatingMessage = nrppa->choice.initiatingMessage;
      auto const& value             = initiatingMessage->value;

      switch (initiatingMessage->procedureCode) {
        case ProcedureCode_id_positioningInformationUpdate: {
          checkPresent(
              value.present,
              InitiatingMessage__value_PR_PositioningInformationUpdate);
          auto const& positioningInformationUpdate =
              value.choice.PositioningInformationUpdate;
        } break;

        case ProcedureCode_id_MeasurementReport: {
          checkPresent(
              value.present, InitiatingMessage__value_PR_MeasurementReport);
          auto const& MeasurementReport = value.choice.MeasurementReport;
        } break;

        case ProcedureCode_id_MeasurementFailureIndication: {
          checkPresent(
              value.present,
              InitiatingMessage__value_PR_MeasurementFailureIndication);
          auto const& measurementFailureIndication =
              value.choice.MeasurementFailureIndication;
        } break;

        default:;
      }
    } break;

    case NRPPA_PDU_PR_successfulOutcome: {
      auto const& successfulOutcome = nrppa->choice.successfulOutcome;
      auto const& value             = successfulOutcome->value;

      switch (successfulOutcome->procedureCode) {
        case ProcedureCode_id_positioningInformationExchange: {
          checkPresent(
              value.present,
              SuccessfulOutcome__value_PR_PositioningInformationResponse);
          auto const& positioningInformationResponse =
              value.choice.PositioningInformationResponse;
          ctx->finish();
          return true;
        } break;

        case ProcedureCode_id_Measurement: {
          checkPresent(
              value.present, SuccessfulOutcome__value_PR_MeasurementResponse);
          auto const& measurementResponse = value.choice.MeasurementResponse;
          ctx->finish();
          return true;
        } break;

        case ProcedureCode_id_positioningActivation: {
          checkPresent(
              value.present,
              SuccessfulOutcome__value_PR_PositioningActivationResponse);
          auto const& positioningActivationResponse =
              value.choice.PositioningActivationResponse;
        } break;

        case ProcedureCode_id_tRPInformationExchange: {
          checkPresent(
              value.present,
              SuccessfulOutcome__value_PR_TRPInformationResponse);
          auto const& TRPInformationResponse =
              value.choice.TRPInformationResponse;
        } break;

        default:;
      }
    } break;

    case NRPPA_PDU_PR_unsuccessfulOutcome: {
      auto const& unsuccessfulOutcome = nrppa->choice.unsuccessfulOutcome;
      auto const& value               = unsuccessfulOutcome->value;

      switch (unsuccessfulOutcome->procedureCode) {
        case ProcedureCode_id_positioningInformationExchange: {
          checkPresent(
              value.present,
              UnsuccessfulOutcome__value_PR_PositioningInformationFailure);
          auto const& PositioningInformationFailure =
              value.choice.PositioningInformationFailure;
        } break;

        case ProcedureCode_id_positioningActivation: {
          checkPresent(
              value.present,
              UnsuccessfulOutcome__value_PR_PositioningActivationFailure);
          auto const& PositioningActivationFailure =
              value.choice.PositioningActivationFailure;
        } break;

        case ProcedureCode_id_Measurement: {
          checkPresent(
              value.present, UnsuccessfulOutcome__value_PR_MeasurementFailure);
          auto const& MeasurementFailure = value.choice.MeasurementFailure;
        } break;

        case ProcedureCode_id_tRPInformationExchange: {
          checkPresent(
              value.present,
              UnsuccessfulOutcome__value_PR_TRPInformationFailure);
          auto const& TRPInformationFailure =
              value.choice.TRPInformationFailure;
        } break;

        default:;
      }
    }
  }

  auto titel  = "n2info nrppa notifiaction pdu error"s;
  auto detail = "unhandled nrppa  pdu: " + std::to_string(nrppa->present);
  throwHttpError(titel, detail);

  return false;
}

// 9.1.1.14 TRP INFORMATION REQUEST
std::pair<asn_encode_to_new_buffer_result_t, lmf_app::gc_c_ptr>
lmf_app::build_trp_information_request_nrppa_pdu() {
  if (this->nrppa_tid_trp_information != 0) {
    nrppa_tid_gen.free_uid(this->nrppa_tid_trp_information);
  }
  // 9.2.4 NRPPa Transaction ID
  auto const nrppatransactionID = NRPPATransactionID_t{
      this->nrppa_tid_trp_information = nrppa_tid_gen.get_uid()};
  // 9.2.24 TRP ID
  auto const ids = std::array<TRP_ID_t, 2>{1, 2};  // c++20: std::to_array
  // TRP Information Type Item's
  auto const informationTypes =
      std::array{TRPInformationTypeItem_nrPCI, TRPInformationTypeItem_geoCoord};
  // TRP List
  auto listIe = TRPInformationRequest_IEs_t{
      .id          = ProtocolIE_ID_id_TRPList,
      .criticality = Criticality_reject,
      .value       = {.present = TRPInformationRequest_IEs__value_PR_TRPList},
  };
  auto list = &listIe.value.choice.TRPList.list;
  // >TRP Item 1 .. <maxnoTRPs>
  auto items = std::array<TRPItem_t, ids.size()>{};
  // >>TRP ID 9.2.24
  for (auto const& [id, item] :
       boost::combine(ids, items)) {  // c++23: std::views::zip
    item = TRPItem_t{.tRP_ID = id};
    ASN_SEQUENCE_ADD(list, &item);
  }
  // TRP Information Type List
  auto informationTypeIe = TRPInformationRequest_IEs_t{
      .id          = ProtocolIE_ID_id_TRPInformationTypeList,
      .criticality = Criticality_reject,
      .value =
          {.present =
               TRPInformationRequest_IEs__value_PR_TRPInformationTypeList},
  };
  auto informationTypeList =
      &informationTypeIe.value.choice.TRPInformationTypeList.list;
  // >TRP Information Type Item 1 .. <maxnoTRPInfoTypes>
  auto informationTypeItems =
      std::array<TRPInformationTypeItem_t, informationTypes.size()>{};
  // >>TRP Information Type ENUMERATED
  for (auto const& [infoType, informationTypeItem] :
       boost::combine(informationTypes, informationTypeItems)) {
    // e_TRPInformationType (enum) to TRPInformationTypeItem_t (long)
    informationTypeItem = infoType;
    ASN_SEQUENCE_ADD(informationTypeList, &informationTypeItem);
  }

  auto initiatingMessage = InitiatingMessage_t{
      .procedureCode      = ProcedureCode_id_tRPInformationExchange,
      .criticality        = Criticality_reject,
      .nrppatransactionID = nrppatransactionID,
      .value = {.present = InitiatingMessage__value_PR_TRPInformationRequest},
  };
  auto informationRequest =
      &initiatingMessage.value.choice.MeasurementRequest.protocolIEs.list;
  ASN_SEQUENCE_ADD(informationRequest, &listIe);
  ASN_SEQUENCE_ADD(informationRequest, &informationTypeIe);

  auto nrppaPdu = NRPPA_PDU_t{
      .present = NRPPA_PDU_PR_initiatingMessage,
      .choice  = {.initiatingMessage = &initiatingMessage},
  };

  xer_fprint(stdout, &asn_DEF_NRPPA_PDU, &nrppaPdu);

  asn_encode_to_new_buffer_result_t rc = asn_encode_to_new_buffer(
      0, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_NRPPA_PDU, &nrppaPdu);

  return {rc, gc_c_ptr{rc.buffer}};
}
