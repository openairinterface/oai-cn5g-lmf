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

#include <boost/range/irange.hpp>
#include <boost/format.hpp>

#include "lmf_app.hpp"
#include "lmf_nrf.hpp"
#include "lmf_client.hpp"

#include "logger.hpp"
#include "conversions.hpp"
#include "mime_parser.hpp"
#include "3gpp_29.518.h"
// model
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
#include "GlobalRanNodeId.h"
// nrppa
#include "InitiatingMessage.h"
#include "SuccessfulOutcome.h"
#include "UnsuccessfulOutcome.h"
#include "ProtocolIE-Field.h"
#include "TRPItem.h"
#include "TRP-MeasurementResponseItem.h"
#include "TrpMeasurementResultItem.h"
#include "TrpMeasuredResultsValue.h"
#include "ULRTOAMeas.h"
#include "UL-RTOAMeasurement.h"
#include "TRPInformationItem.h"

using namespace std;
using namespace oai::lmf::app;
using namespace oai::lmf_server::model;
using namespace config;

lmf_client* lmf_client_inst = nullptr;
lmf_nrf* lmf_nrf_inst       = nullptr;

// provides for asn container.list.array range based for loops
// for (auto const& xyzIE : xyzResponse.protocolIEs) {
template<typename T>
auto begin(T const& container) {
  return container.list.array;
}

template<typename T>
auto end(T const& container) {
  return container.list.array + container.list.count;
}

static NRPPA_PDU_t* build_trp_information_response(
    NRPPATransactionID_t const& nrppaTxnId) {
  auto successfulTrpInformationExchange =
      (SuccessfulOutcome_t*) malloc(sizeof(SuccessfulOutcome_t));
  *successfulTrpInformationExchange = SuccessfulOutcome_t{
      .procedureCode      = ProcedureCode_id_tRPInformationExchange,
      .criticality        = Criticality_reject,
      .nrppatransactionID = nrppaTxnId,
      .value =
          {
              .present = SuccessfulOutcome__value_PR_TRPInformationResponse,
          },
  };
  auto trpInformationIEs = &successfulTrpInformationExchange->value.choice
                                .TRPInformationResponse.protocolIEs.list;

  auto trpInformationListIE = (TRPInformationResponse_IEs_t*) malloc(
      sizeof(TRPInformationResponse_IEs_t));
  *trpInformationListIE = TRPInformationResponse_IEs_t{
      .id          = ProtocolIE_ID_id_TRPInformationList,
      .criticality = Criticality_reject,
      .value =
          {
              .present =
                  TRPInformationResponse_IEs__value_PR_TRPInformationList,
          },
  };
  ASN_SEQUENCE_ADD(trpInformationIEs, trpInformationListIE);
  auto trpInformationList =
      &trpInformationListIE->value.choice.TRPInformationList;

  for (TRP_ID_t trpId : boost::irange(1, 4)) {
    auto trpInformationListMember = (TRPInformationList__Member*) malloc(
        sizeof(TRPInformationList__Member));
    *trpInformationListMember = TRPInformationList__Member{
        .tRP_ID = trpId,
    };
    ASN_SEQUENCE_ADD(trpInformationList, trpInformationListMember);
    auto trpInformationItemList =
        &trpInformationListMember->tRPInformation.list;

    // 28bit gnbId 0x400 (1024dez) 8bit cellId 0x10 (16dez) (36bit)
    // 9.2.9 NR CGI
    auto ngRanCgi = (NG_RAN_CGI_t*) malloc(sizeof(NG_RAN_CGI_t));
    *ngRanCgi     = NG_RAN_CGI_t{
        .pLMN_Identity =
            PLMN_Identity_t{
                .buf  = (uint8_t*) malloc(3),
                .size = 3,
            },
        .nG_RANcell =
            NG_RANCell_t{
                .present = NG_RANCell_PR_nR_CellID,
                .choice =
                    {
                        .nR_CellID =
                            NRCellIdentifier_t{
                                .buf         = (uint8_t*) malloc(5),
                                .size        = 5,
                                .bits_unused = 4,  // 36bits
                            },
                    },
            },
    };
    // 9.2.8 PLMN Identity
    auto& plmnIdentity  = ngRanCgi->pLMN_Identity;
    plmnIdentity.buf[0] = 0x0d;  // first 2 digits mcc 0x0d0 = 208
    plmnIdentity.buf[1] = 0x0f;  // third digit mcc + filler
    plmnIdentity.buf[2] = 0x5f;  // mnc 2 digits 0x5f = 95
    // NR Cell Identity BIT STRING (SIZE(36))
    auto& nrCellId = ngRanCgi->nG_RANcell.choice.nR_CellID;
    // gnbId + trpId shift left 8bit cellId
    uint64_t nci = (0x40010 + (trpId < 3 ? trpId << 8 : (trpId - 1) << 8))
                   << nrCellId.bits_unused;
    {
      auto i = 0;
      for (auto const& s : boost::irange(32, -1, -8)) {
        nrCellId.buf[i++] = nci >> s;
      }
    }
    auto trpInformationItem =
        (TRPInformationItem_t*) malloc(sizeof(TRPInformationItem_t));
    *trpInformationItem = TRPInformationItem_t{
        .present = TRPInformationItem_PR_nG_RAN_CGI,
        .choice{
            .nG_RAN_CGI = ngRanCgi,
        },
    };
    ASN_SEQUENCE_ADD(trpInformationItemList, trpInformationItem);
  }

  auto nrppaPdu = (NRPPA_PDU_t*) malloc(sizeof(NRPPA_PDU_t));
  *nrppaPdu     = NRPPA_PDU_t{
      .present = NRPPA_PDU_PR_successfulOutcome,
      .choice =
          {
              .successfulOutcome = successfulTrpInformationExchange,
          },
  };

  xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppaPdu);
  asn_encode_to_new_buffer_result_t nrppaPduEnc = asn_encode_to_new_buffer(
      0, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_NRPPA_PDU, nrppaPdu);
  if (nrppaPduEnc.result.encoded == -1) {
    Logger::lmf_app().error(
        "Could not encode (at %s)\n", nrppaPduEnc.result.failed_type ?
                                          nrppaPduEnc.result.failed_type->name :
                                          "unknown");
  }
  free(nrppaPduEnc.buffer);

  return nrppaPdu;
}

//------------------------------------------------------------------------------
lmf_app::lmf_app(const std::string& config_file, lmf_event& ev)
    : event_sub(ev) {
  // fake TRP Information Response
  // this->handle_non_ue_n2info_nrppa_notification(build_trp_information_response(this->nrppa_tid_trp_information=1));

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

  this->nonUeN2MessageSubscription =
      std::make_unique<NonUeN2MessageSubscription>();
  if (lmf_cfg.request_trp_info) {
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
  auto const& pir_tId = this->nrppa_tid_gen.get_uid();
  ctx->positioning_information_request(pir_tId);
  auto const& [nrppaPduPIR, positioningInformationResponse, srsConfiguration] =
      ctx->positioning_information_response.get_future().get();
#if 0  // at gNb not implemented 
  // 5. NRPPa Request UE SRS activation
  // 9.1.1.17 POSITIONING ACTIVATION REQUEST
  auto const& pa_tId = this->nrppa_tid_gen.get_uid();
  ctx->positioning_activation_request(pa_tId);
  auto const& [nrppaPduPA, positionActivationResponse] =
      ctx->positioning_activation_response.get_future().get();
#endif
  auto const& mr_tId = this->nrppa_tid_gen.get_uid();
  ctx->measurement_request(mr_tId, srsConfiguration);
  // for (auto& prom : ctx->resps) { // boost::wait_for_all
  //   prom.get_future().wait();
  // }
  auto f = ctx->resps.at(0).get_future();
  f.wait();
  auto const& [nrppaPduMR, measurementResponse] = f.get();

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

  // for each trp_id a map of 9.2.39 UL RTOA Measurement
  std::map<TRP_ID_t, std::map<ULRTOAMeas_PR, long>> res;
  for (auto const& measurementIE : measurementResponse.protocolIEs) {
    if (measurementIE->id == ProtocolIE_ID_id_TRP_MeasurementResponseList &&
        measurementIE->value.present ==
            MeasurementResponse_IEs__value_PR_TRP_MeasurementResponseList) {
      auto const trpMeasurementList =
          measurementIE->value.choice.TRP_MeasurementResponseList;
      for (auto const& trpMeasurement : trpMeasurementList) {
        auto const& trpId = trpMeasurement->tRP_ID;
        for (auto const& measurement : trpMeasurement->measurementResult) {
          if (measurement->measuredResultsValue.present ==
              TrpMeasuredResultsValue_PR_uL_RTOA) {
            auto const& uLRTOAmeas =
                measurement->measuredResultsValue.choice.uL_RTOA->uLRTOAmeas;
            auto const& choice = uLRTOAmeas.choice;
            auto const& key    = uLRTOAmeas.present;
            auto const& val    = key == ULRTOAMeas_PR_k0 ? choice.k0 :
                                 key == ULRTOAMeas_PR_k1 ? choice.k1 :
                                 key == ULRTOAMeas_PR_k2 ? choice.k2 :
                                 key == ULRTOAMeas_PR_k3 ? choice.k3 :
                                 key == ULRTOAMeas_PR_k4 ? choice.k4 :
                                                           choice.k5;
            res[trpId].insert({key, val});
          }
        }
      }
    }
  }

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

  // release_n1n2subscription(supi);
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

NRPPATransactionID_t getNrppaTxnId(NRPPA_PDU_t const* const nrppa) {
  switch (nrppa->present) {
    case NRPPA_PDU_PR_initiatingMessage:
      return nrppa->choice.initiatingMessage->nrppatransactionID;
    case NRPPA_PDU_PR_successfulOutcome:
      return nrppa->choice.successfulOutcome->nrppatransactionID;
    case NRPPA_PDU_PR_unsuccessfulOutcome:
      return nrppa->choice.unsuccessfulOutcome->nrppatransactionID;
    default:
      throwHttpError("getNrppaTxnId"s, "malformed nrppa message"s);
  }
  return 0;
}

// check 1:1 relationship between procedureCode and value.present
template<typename T, typename U>
static void checkPC(
    std::string const& ux, T const& present, U const& expected) {
  if (present->procedureCode != expected) {
    auto const &title = "handle_" + ux +
                        "_n2info_nrppa_notification: invalid procedue code"s,
               &ps     = "present: "s + std::to_string(present->procedureCode),
               &es     = "expected: "s + std::to_string(expected),
               &detail = ps + ": "s + es;
    throwHttpError(title, detail);
  }
}

template<typename T, typename U, typename V>
static U const& getPR(
    std::string const& ux, U const& choice, T const& value, V const& expected) {
  if (value.present != expected) {
    auto const &title = "handle_" + ux +
                        "_n2info_nrppa_notification: invalid message"s,
               &ps     = "present: "s + std::to_string(value.present),
               &es     = "expected: "s + std::to_string(expected),
               &detail = ps + ": "s + es;
    throwHttpError(title, detail);
  }
  return choice;
}

bool lmf_app::handle_non_ue_n2info_nrppa_notification(NRPPA_PDU_t* nrppa) {
  auto const& nrppaTxnId = getNrppaTxnId(nrppa);

  if (nrppaTxnId != this->nrppa_tid_trp_information) {
    if (this->supiByNrppaTxnId.count(nrppaTxnId) == 0) {
      throwHttpError(
          "handle_non_ue_n2info_nrppa_notification",
          "unknown nrppa txnid: "s + std::to_string(nrppaTxnId));
    }

    auto const supi = this->supiByNrppaTxnId.at(nrppaTxnId);
    this->supiByNrppaTxnId.erase(nrppaTxnId);

    return this->handle_n2info_nrppa_notification(supi, nrppa);
  } else {
    Logger::lmf_app().debug("trp information received");

    auto const& successfullTrpInformationExchange = getPR(
        "non_ue"s, nrppa->choice.successfulOutcome, *nrppa,
        NRPPA_PDU_PR_successfulOutcome);
    checkPC(
        "non_ue"s, successfullTrpInformationExchange,
        ProcedureCode_id_tRPInformationExchange);

    auto const& trpInformationExchange =
        successfullTrpInformationExchange->value;
    auto const& trpInformation = getPR(
        "non_ue"s, trpInformationExchange.choice.TRPInformationResponse,
        trpInformationExchange,
        SuccessfulOutcome__value_PR_TRPInformationResponse);
    for (auto const& trpInformationIE : trpInformation.protocolIEs) {
      if (trpInformationIE->id == ProtocolIE_ID_id_TRPInformationList) {
        auto const& value              = trpInformationIE->value;
        auto const& trpInformationList = getPR(
            "non_ue"s, value.choice.TRPInformationList, value,
            TRPInformationResponse_IEs__value_PR_TRPInformationList);
        for (auto const& trpInformationListMember : trpInformationList) {
          auto const& trpId = trpInformationListMember->tRP_ID;
          for (auto const& trpInformationItem :
               trpInformationListMember->tRPInformation) {
            if (trpInformationItem->present ==
                TRPInformationItem_PR_nG_RAN_CGI) {
              auto const& ngRanCgi      = trpInformationItem->choice.nG_RAN_CGI;
              auto const& plmnnIdentity = ngRanCgi->pLMN_Identity;
              if (plmnnIdentity.size != 3) {
                throwHttpError(
                    "trp information response",
                    "plmnnIdentity.size != 3: "s +
                        std::to_string(plmnnIdentity.size));
              }
              auto const& mcc_ =
                  (plmnnIdentity.buf[0] << 8 | plmnnIdentity.buf[1]) >> 4;
              auto const& msdMnc      = plmnnIdentity.buf[1] << 4;
              auto const& twoDigitMnc = msdMnc == 0xf;  // filler
              auto const& mnc_        = twoDigitMnc ?
                                            plmnnIdentity.buf[2] :
                                            msdMnc << 8 | plmnnIdentity.buf[2];

              if (ngRanCgi->nG_RANcell.present == NG_RANCell_PR_nR_CellID) {
                auto const& ngRanCell = ngRanCgi->nG_RANcell.choice.nR_CellID;
                if (ngRanCell.size != 5 || ngRanCell.bits_unused != 4) {
                  throwHttpError(
                      "trp information response",
                      "ngRanCell.size != 5: "s +
                          std::to_string(ngRanCell.size) +
                          " || ngRanCell.bits_unused != 4: " +
                          std::to_string(ngRanCell.bits_unused));
                }
                auto nci = uint64_t{0};
                {
                  auto i = 0;
                  for (auto const& s : boost::irange(32, -1, -8)) {
                    nci |= ngRanCell.buf[i++] << s;
                  }
                }
                nci >>= ngRanCell.bits_unused;
                auto const nciGnbIdBitCnt = 28;
                auto const cellIdBitCnt   = 36 - nciGnbIdBitCnt;
                auto const& gnbId         = nci >> cellIdBitCnt;

                if (this->gnb.count(gnbId) == 0) {
                  auto const& mcc = (boost::format("%03x") % mcc_).str();
                  auto const& mnc =
                      (boost::format(twoDigitMnc ? "%02x" : "%03x") % mnc_)
                          .str();
                  PlmnId plmnId;
                  plmnId.setMcc(mcc);
                  plmnId.setMnc(mnc);

                  auto const& gnbValue = (boost::format("%x") % gnbId).str();
                  GNbId gNbId;
                  gNbId.setGNBValue(gnbValue);
                  gNbId.setBitLength(nciGnbIdBitCnt);

                  GlobalRanNodeId globalRanNodeId;
                  globalRanNodeId.setPlmnId(plmnId);
                  globalRanNodeId.setGNbId(gNbId);

                  this->gnb.try_emplace(gnbId, globalRanNodeId);
                }
                if (this->gnb.at(gnbId).trpIds.count(trpId) == 0) {
                  this->gnb.at(gnbId).trpIds.insert(trpId);
                } else {
                  Logger::lmf_app().warn(
                      "trp information: gnb_id: " + std::to_string(gnbId) +
                      "trp_id: " + std::to_string(trpId) + " not unique");
                }
              }
            }
          }
        }
      }
    }

    ASN_STRUCT_FREE(asn_DEF_NRPPA_PDU, nrppa);
  }

  return false;
}

// TODO: replace bool retval with exception
// shoult not fail
bool lmf_app::handle_n2info_nrppa_notification(
    std::string supi, NRPPA_PDU_t* nrppa) {
  auto ctx = this->supi_2_context(supi);
  if (!ctx) {
    Logger::lmf_server().error("N2InfoNotify: unknown supi: %s", supi);
    return false;
  }

  auto const& tId = getNrppaTxnId(nrppa);

  // TODO
  if (nrppa->present == NRPPA_PDU_PR_initiatingMessage) {
    throwHttpError(
        "handle_n2info_nrppa_notification",
        "NRPPA_PDU_PR_initiatingMessage not implemented");
  }

  if (ctx->nrppa_tId.count(tId) != 1) {
    throwHttpError(
        "handle_n2info_nrppa_notification"s,
        "unknown nrppa transaction id: "s + std::to_string(tId));
  }

  auto const procedureCode = ctx->nrppa_tId.at(tId);
  ctx->nrppa_tId.erase(tId);          // not for incomming!
  this->nrppa_tid_gen.free_uid(tId);  // for reuse

  if (nrppa->present == NRPPA_PDU_PR_unsuccessfulOutcome) {
    throwHttpError(
        "handle_n2info_nrppa_notification",
        "NRPPA_PDU_PR_unsuccessfulOutcome not implemented");
  }

  auto const& successfulOutcome = getPR(
      "ue"s, nrppa->choice.successfulOutcome, *nrppa,
      NRPPA_PDU_PR_successfulOutcome);
  switch (procedureCode) {
    case ProcedureCode_id_positioningInformationExchange: {
      checkPC("ue"s, successfulOutcome, procedureCode);
      auto const& value                          = successfulOutcome->value;
      auto const& positioningInformationResponse = getPR(
          "ue"s, value.choice.PositioningInformationResponse, value,
          SuccessfulOutcome__value_PR_PositioningInformationResponse);
      ctx->handle_positioning_information_response(
          nrppa, tId, positioningInformationResponse);
      return true;
    } break;

    case ProcedureCode_id_Measurement: {
      checkPC("ue"s, successfulOutcome, procedureCode);
      auto const& value               = successfulOutcome->value;
      auto const& measurementResponse = getPR(
          "ue"s, value.choice.MeasurementResponse, value,
          SuccessfulOutcome__value_PR_MeasurementResponse);
      ctx->handle_measurement_response(nrppa, tId, measurementResponse);
      return true;
    } break;

    case ProcedureCode_id_positioningActivation: {
      checkPC("ue"s, successfulOutcome, procedureCode);
      auto const& value                         = successfulOutcome->value;
      auto const& positioningActivationResponse = getPR(
          "ue"s, value.choice.PositioningActivationResponse, value,
          SuccessfulOutcome__value_PR_PositioningActivationResponse);
      ctx->handle_positioning_activation_response(
          nrppa, tId, positioningActivationResponse);
      return true;
    } break;

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
  // 9.2.4 NRPPa Transaction ID
  auto const nrppatransactionID = NRPPATransactionID_t{
      this->nrppa_tid_trp_information = nrppa_tid_gen.get_uid()};

  auto initiatingMessage =
      (InitiatingMessage_t*) malloc(sizeof(InitiatingMessage_t));
  auto nrppaPdu = (NRPPA_PDU_t*) malloc(sizeof(NRPPA_PDU_t));
  *nrppaPdu     = NRPPA_PDU_t{
      .present = NRPPA_PDU_PR_initiatingMessage,
      .choice =
          {
              .initiatingMessage = initiatingMessage,
          },
  };

  *initiatingMessage = InitiatingMessage_t{
      .procedureCode      = ProcedureCode_id_tRPInformationExchange,
      .criticality        = Criticality_reject,
      .nrppatransactionID = nrppatransactionID,
      .value =
          {
              .present = InitiatingMessage__value_PR_TRPInformationRequest,
              .choice =
                  {
                      .TRPInformationRequest = TRPInformationRequest_t{},
                  },
          },
  };

  xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppaPdu);

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
