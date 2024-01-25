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
#include "N2InformationNotification.h"

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
                ":" + std::to_string(lmf_cfg.amf_addr.port) + NAMF_BASE +
                lmf_cfg.sbi_api_version + "/non-ue-n2-messages/transfer";
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
  // TODO: move nrppa_tid_gen to LocationDetermination
  // and use RAII (unique_ptr) for auto free_uid()
  auto const& pir_tId = this->nrppa_tid_gen.get_uid();
  ctx->positioning_information_request(pir_tId);
  auto const& [nrppaPduPIR, positioningInformationResponse] =
      ctx->positioning_information_response.get_future().get();
  // don't free tId, avoid re-use for easier debugging
  // this->nrppa_tid_gen.free_uid(tId); // after response handle done
  // stay subscribed
  // release_n1n2subscription(supi);
#if 0  // at gNb not implemented 
  // 5. NRPPa Request UE SRS activation
  // 9.1.1.17 POSITIONING ACTIVATION REQUEST
  auto const& pa_tId = this->nrppa_tid_gen.get_uid();
  ctx->positioning_activation_request(pa_tId);
  auto const& [nrppaPduPA, positionActivationResponse] =
      ctx->positioning_activation_response.get_future().get();
#endif
  auto const& mr_tId = this->nrppa_tid_gen.get_uid();
  ctx->measurement_request(mr_tId);
  auto const& [nrppaPduMR, measurementResponse] =
      ctx->measurement_response.get_future().get();

  // nrppaPduPIR contain position information
  // POSITIONING INFORMATION RESPONSE ( 9.1.1.11 NRPPa TS 38.455 )
  std::cout << "--> position information <<--" << std::endl;
  xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppaPduPIR);

  // TRP INFORMATION RESPONSE ( 9.1.1.15 NRPPa TS 38.455 )
  // not availalbe right now, because no AMF non-ue-message-service

  // nrppaPduMR contain measurement
  // MEASUREMENT RESPONSE ( 9.1.4.2 NRPPa TS 38.455 )
  std::cout << "--> measurement <<--" << std::endl;
  xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppaPduMR);

  // --> calculate position here <--
  // double position_estimation(
  //    double trp_pos[][3], int trp_pos_size,
  //    double dd_estimated[], int dd_estimated_size,
  //    double pos_est[]);

  // --> set the location calculation results here <--
  LocationData locationData;
  locationData.setBarometricPressure(1);

  code      = Pistache::Http::Code::Ok;
  json_data = locationData;

  ASN_STRUCT_FREE(asn_DEF_NRPPA_PDU, nrppaPduPIR);
  ASN_STRUCT_FREE(asn_DEF_NRPPA_PDU, nrppaPduMR);
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
static void check(T const& present, U const& expected) {
  if (present != expected) {
    auto const &title  = "handle_n2info_nrppa_notification: invalid message"s,
               &ps     = "present: "s + std::to_string(present),
               &es     = "expected: "s + std::to_string(expected),
               &detail = ps + ": "s + es;
    throwHttpError(title, detail);
  }
}

template<typename T, typename U, typename V>
static U const& get(U const& choice, T const& present, V const& expected) {
  if (present != expected) {
    auto const &title  = "handle_n2info_nrppa_notification: invalid message"s,
               &ps     = "present: "s + std::to_string(present),
               &es     = "expected: "s + std::to_string(expected),
               &detail = ps + ": "s + es;
    throwHttpError(title, detail);
  }
  return choice;
}

NRPPATransactionID_t getNrppaId(NRPPA_PDU_t const* const nrppa) {
  switch (nrppa->present) {
    case NRPPA_PDU_PR_initiatingMessage:
      return nrppa->choice.initiatingMessage->nrppatransactionID;
    case NRPPA_PDU_PR_successfulOutcome:
      return nrppa->choice.successfulOutcome->nrppatransactionID;
    case NRPPA_PDU_PR_unsuccessfulOutcome:
      return nrppa->choice.unsuccessfulOutcome->nrppatransactionID;
    default:
      throwHttpError("getNrppaId"s, "malformed nrppa message"s);
  }
  return 0;
}

bool lmf_app::handle_n2info_nrppa_notification(
    std::string supi, NRPPA_PDU_t* nrppa) {
  auto ctx = this->supi_2_context(supi);
  if (!ctx) {
    Logger::lmf_server().error("N2InfoNotify: unknown supi: %s", supi);
    return false;
  }

  auto const& tId = getNrppaId(nrppa);
  if (ctx->nrppa_tId.count(tId) != 1) {
    throwHttpError(
        "handle_n2info_nrppa_notification"s,
        "unknown nrppa transaction id: "s + std::to_string(tId));
  }

  auto const& successfulOutcome =
      get(nrppa->choice.successfulOutcome, nrppa->present,
          NRPPA_PDU_PR_successfulOutcome);
  switch (ctx->nrppa_tId.at(tId)) {
    case ResponseType::PositionInformation: {
      check(
          successfulOutcome->procedureCode,
          ProcedureCode_id_positioningInformationExchange);
      auto const& value = successfulOutcome->value;
      auto const& positioningInformationResponse =
          get(value.choice.PositioningInformationResponse, value.present,
              SuccessfulOutcome__value_PR_PositioningInformationResponse);
      ctx->handle_positioning_information_response(
          nrppa, tId, positioningInformationResponse);
      return true;
    } break;

    case ResponseType::Measurement: {
      check(successfulOutcome->procedureCode, ProcedureCode_id_Measurement);
      auto const& value = successfulOutcome->value;
      auto const& measurementResponse =
          get(value.choice.MeasurementResponse, value.present,
              SuccessfulOutcome__value_PR_MeasurementResponse);
      ctx->handle_measurement_response(nrppa, tId, measurementResponse);
      return true;
    } break;

    case ResponseType::PositioningActivation: {
      check(
          successfulOutcome->procedureCode,
          ProcedureCode_id_positioningActivation);
      auto const& value = successfulOutcome->value;
      auto const& positioningActivationResponse =
          get(value.choice.PositioningActivationResponse, value.present,
              SuccessfulOutcome__value_PR_PositioningActivationResponse);
      ctx->handle_positioning_activation_response(
          nrppa, tId, positioningActivationResponse);
      return true;
    }

    default:
      throwHttpError(
          "handle_n2info_nrppa_notification"s, "unhandled response type"s);
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
  for (auto const& [id, _item] :
       boost::combine(ids, items)) {  // c++23: std::views::zip
#if BOOST_VERSION / 100 % 1000 >= 74  // ubuntu 22
    auto& item = _item;
#else  // ubuntu 20 / rhel8
    auto& item                = boost::get<0>(_item);
#endif
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
  for (auto const& [infoType, informationTypeItem_] :
       boost::combine(informationTypes, informationTypeItems)) {
#if BOOST_VERSION / 100 % 1000 >= 74  // ubuntu 22
    auto& informationTypeItem = informationTypeItem_;
#else  // ubuntu 20 / rhel8
    auto& informationTypeItem = boost::get<0>(informationTypeItem_);
#endif
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

NRPPA_PDU_t* lmf_app::parse_n2_info_container_nrppa(
    N2InformationNotification const& n2InformationNotification,
    mime_part const& nrppa_part) {
  if (!n2InformationNotification.n2InfoContainerIsSet()) {
    throwHttpError(
        "parse_n2_info_container_nrppa", "N2InfoContainer not present");
  }

  auto const& n2InfoContainer = n2InformationNotification.getN2InfoContainer();
  auto const& eN2InformationClass =
      n2InfoContainer.getN2InformationClass().getEnumValue();

  // Check N2 Information Class
  if (eN2InformationClass !=
      N2InformationClass_anyOf::eN2InformationClass_anyOf::NRPPA) {
    throwHttpError(
        "parse_n2_info_container_nrppa",
        "N2 Information Class not NRPPA: " +
            std::to_string(static_cast<int>(eN2InformationClass)));
  }

  if (!n2InfoContainer.nrppaInfoIsSet()) {
    throwHttpError("parse_n2_info_container_nrppa", "nrppaInfo not present");
  }
  auto const& nrppaInfo = n2InfoContainer.getNrppaInfo();

  if (nrppaInfo.getNfId() != lmf_nrf_inst->lmf_instance_id) {
    Logger::lmf_server().warn(
        "nfId != '%s': '%s'", lmf_nrf_inst->lmf_instance_id,
        nrppaInfo.getNfId());
  }

  auto const& nrppaPdu = nrppaInfo.getNrppaPdu();
  if (!nrppaPdu.ngapIeTypeIsSet()) {
    throwHttpError("parse_n2_info_container_nrppa", "ngapIeType not present");
  }

  auto const& eNgapIeType = nrppaPdu.getNgapIeType().getEnumValue();
  if (eNgapIeType != NgapIeType_anyOf::eNgapIeType_anyOf::NRPPA_PDU) {
    throwHttpError(
        "parse_n2_info_container_nrppa",
        "ngapIeType not NRPPA_PDU: " +
            std::to_string(static_cast<int>(eNgapIeType)));
  }
  auto const& ngapData = nrppaPdu.getNgapData();
  Logger::lmf_app().debug(
      "parse_n2_info_container_nrppa: content-id: " + ngapData.getContentId());
  if (nrppa_part.content_type != "application/vnd.3gpp.ngap") {
    Logger::lmf_server().warn(
        "content-type != 'application/vnd.3gpp.ngap': '%s'",
        nrppa_part.content_type);
  }

  auto const& nrppa_bin = nrppa_part.body;
  NRPPA_PDU_t* nrppa = nullptr;  // TODO: warp in unigue_ptr with custom deleter
  auto const& rc     = asn_decode(
      NULL, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_NRPPA_PDU, (void**) &nrppa,
      nrppa_bin.c_str(), nrppa_bin.length());
  if (rc.code != RC_OK) {
    ASN_STRUCT_FREE(asn_DEF_NRPPA_PDU, nrppa);
    throwHttpError(
        "parse_n2_info_container_nrppa",
        "asn_decode failed: " + std::to_string(rc.code));
  }
  xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppa);
  Logger::lmf_server().debug("asn_decode ok, consumed: %d", rc.consumed);

  return nrppa;
}