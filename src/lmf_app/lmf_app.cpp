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
#include <chrono>
using namespace std::chrono_literals;
#include <thread>

#include <boost/range/irange.hpp>
#include <boost/format.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/lambda/lambda.hpp>

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

  Logger::lmf_app().startup("Started");
}

//------------------------------------------------------------------------------
lmf_app::~lmf_app() {
  Logger::lmf_app().debug("Delete LMF_APP instance...");
}

void lmf_app::trp_information_request(
    std::shared_ptr<LocationDetermination> const& ctx,
    NRPPATransactionID_t const& nrppatransactionID) {
  Logger::lmf_app().info("trp information request");

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

  // xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppaPdu);

  asn_encode_to_new_buffer_result_t rc = asn_encode_to_new_buffer(
      0, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_NRPPA_PDU, nrppaPdu);

  ctx->non_ue_n2_message_transfer(
      nrppaPdu, nrppatransactionID, ProcedureCode_id_tRPInformationExchange, {},
      nullptr);
}

void lmf_app::trp_information(
    std::shared_ptr<LocationDetermination> const& ctx) {
  std::unique_lock lk{this->cv_m_gnb};

  if (this->gnb.size() > 0) {
    Logger::lmf_app().debug(
        "trp information request: alreadey done: gnbs: %d trps: %d",
        this->gnb.size(), this->numTrps());
    return;
  }

  auto const& tId = this->nrppa_tid_gen.get_uid();
  this->trp_information_request(ctx, tId);

  // lmf_cfg.determine_num_gnb -> return false to wait until trp_info_wait_ms
  this->trp_info_err.clear();
  auto const& pred = [&gnb = this->gnb, &err = this->trp_info_err] {
    if (lmf_cfg.determine_num_gnb) return false;  // wait for timeout
    return (gnb.size() + err.size()) ==
           lmf_cfg.num_gnb;  // stop if cfg'd gnb's recvd
  };
  Logger::lmf_app().debug(
      "trp information request: wait %dms for %s gnb responses",
      lmf_cfg.trp_info_wait_ms.count(),
      lmf_cfg.determine_num_gnb ? "until timeout" :
                                  std::to_string(lmf_cfg.num_gnb));
  auto const& rc = this->cv_gnb.wait_for(lk, lmf_cfg.trp_info_wait_ms, pred);
  ctx->nrppa_tId.erase(tId);
  this->nrppa_tid_gen.free_uid(tId);
  if (this->trp_info_err.size() > 0) {
    throwHttpError(
        "trp information failure",
        "gnb err count: "s + std::to_string(this->trp_info_err.size()));
  }
  if (!lmf_cfg.determine_num_gnb &&
      (!rc || this->gnb.size() < lmf_cfg.num_gnb)) {
    throwHttpError(
        "trp information request",
        "timeout after "s + std::to_string(lmf_cfg.trp_info_wait_ms.count()) +
            "ms waiting for "s + std::to_string(lmf_cfg.num_gnb) +
            " gnb responses, but received: "s +
            std::to_string(this->gnb.size()));
  }
  Logger::lmf_app().debug(
      "trp information request: received %d gnb responses", this->gnb.size());

  if (this->gnb.size() == 0) {
    throwHttpError("trp information request"s, "no gnbs available"s);
  }
  if (this->numTrps() == 0) {
    throwHttpError("trp information request"s, "no trp's available"s);
  }
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

  this->create_non_ue_subscription();
  this->trp_information(ctx);

  this->create_n1n2subscription(supi);
  auto const& pir_tId = this->nrppa_tid_gen.get_uid();
  ctx->positioning_information_request(pir_tId);
  auto const& [nrppaPduPIR, positioningInformationResponse, ueSrsConfiguration] =
      ctx->positioning_information_response.get_future().get();
  // nrppaPduPIR contain position information
  // POSITIONING INFORMATION RESPONSE ( 9.1.1.11 NRPPa TS 38.455 )
  std::cout << "--> position information <<--" << std::endl;
  // xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppaPduPIR);

  // 5. NRPPa Request UE SRS activation
  // 9.1.1.17 POSITIONING ACTIVATION REQUEST
  auto const& pa_tId = this->nrppa_tid_gen.get_uid();
  ctx->positioning_activation_request(pa_tId);
  auto const& [nrppaPduPA, positionActivationResponse] =
      ctx->positioning_activation_response.get_future().get();
  ASN_STRUCT_FREE(asn_DEF_NRPPA_PDU, nrppaPduPA);
  std::cout << "--> positioning activation <<--" << std::endl;
  for (auto const& [id, gnb] : this->gnb) {
    auto const& mr_tId            = this->nrppa_tid_gen.get_uid();
    auto const& mId               = lmf_app_inst->measurement_id_gen.get_uid();
    auto const& globalRanNodeList = std::vector{gnb.ncgi};
    auto const& trpIdRng          = boost::adaptors::keys(gnb.trp);
    auto const& trpIds            = std::set(trpIdRng.begin(), trpIdRng.end());
    auto f                        = ctx->measurement_request(
        mr_tId, mId, globalRanNodeList, trpIds, ueSrsConfiguration);
    f.wait();
    auto const& [nrppaPduMR, measurementResponse] = f.get();
    this->measurement_id_gen.free_uid(mId);
    // nrppaPduMR contain measurement
    // MEASUREMENT RESPONSE ( 9.1.4.2 NRPPa TS 38.455 )
    std::cout << "--> measurement <<--" << std::endl;
    // xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppaPduMR);

    // for each trp_id a map of 9.2.39 UL RTOA Measurement
    std::map<GnbId, std::map<TRP_ID_t, std::map<ULRTOAMeas_PR, long>>> res;
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
              if (auto const& [iter, inserted] =
                      res[id][trpId].try_emplace(key, val);
                  !inserted) {
                throwHttpError("measurement", "double value");
              }
            }
          }
        }
      }
    }

    for (auto const& [gndId, trp] : res) {
      std::cout << "gndId: " << gndId << std::endl;
      for (auto const& [trpId, uLRTOAmeas] : trp) {
        std::cout << "trpId: " << trpId << std::endl;
        for (auto const& [k, v] : uLRTOAmeas) {
          std::cout << "k: " << k << " v: " << v << std::endl;
        }
      }
    }

    ASN_STRUCT_FREE(asn_DEF_NRPPA_PDU, nrppaPduMR);
  }

  // TRP INFORMATION RESPONSE ( 9.1.1.15 NRPPa TS 38.455 )
  // not availalbe right now, because no AMF non-ue-message-service

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

void lmf_app::create_non_ue_subscription() {
  std::scoped_lock lock(this->m_non_ue_subs);

  if (!this->nonUeN2MessageSubscription) {
    this->nonUeN2MessageSubscription =
        std::make_unique<NonUeN2MessageSubscription>();
    Logger::lmf_app().info("non-ue subscription created");
  } else {
    Logger::lmf_app().debug(
        "non-ue subscription not created: already subscribed");
  }
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
static void checkPC(T const& present, U const& expected) {
  if (present->procedureCode != expected) {
    auto const &title  = "handle_nrppa_notification: invalid procedue code"s,
               &ps     = "present: "s + std::to_string(present->procedureCode),
               &es     = "expected: "s + std::to_string(expected),
               &detail = ps + ": "s + es;
    throwHttpError(title, detail);
  }
}

template<typename T, typename U, typename V>
static U const& getPR(U const& choice, T const& value, V const& expected) {
  if (value.present != expected) {
    auto const &title  = "handle_nrppa_notification: invalid message"s,
               &ps     = "present: "s + std::to_string(value.present),
               &es     = "expected: "s + std::to_string(expected),
               &detail = ps + ": "s + es;
    throwHttpError(title, detail);
  }
  return choice;
}

void lmf_app::handle_trp_information_response(
    NRPPA_PDU_t* nrppa, NRPPATransactionID_t const& tId,
    TRPInformationResponse_t const& trpInformation) {
  Logger::lmf_app().debug("trp information received");
  std::scoped_lock lk{this->cv_m_gnb};

  for (auto const& trpInformationIE : trpInformation.protocolIEs) {
    if (trpInformationIE->id == ProtocolIE_ID_id_TRPInformationList) {
      auto const& value              = trpInformationIE->value;
      auto const& trpInformationList = getPR(
          value.choice.TRPInformationList, value,
          TRPInformationResponse_IEs__value_PR_TRPInformationList);
      for (auto const& trpInformationListMember : trpInformationList) {
        auto const& trpId = trpInformationListMember->tRP_ID;
        for (auto const& trpInformationItem :
             trpInformationListMember->tRPInformation) {
          if (trpInformationItem->present == TRPInformationItem_PR_nG_RAN_CGI) {
            auto const& ngRanCgi      = trpInformationItem->choice.nG_RAN_CGI;
            auto const& plmnnIdentity = ngRanCgi->pLMN_Identity;
            if (plmnnIdentity.size != 3) {
              throwHttpError(
                  "trp information response",
                  "plmnnIdentity.size != 3: "s +
                      std::to_string(plmnnIdentity.size));
            }

            if (ngRanCgi->nG_RANcell.present == NG_RANCell_PR_nR_CellID) {
              auto const& ngRanCell = ngRanCgi->nG_RANcell.choice.nR_CellID;
              if (ngRanCell.size != 5 || ngRanCell.bits_unused != 4) {
                throwHttpError(
                    "trp information response",
                    "ngRanCell.size != 5: "s + std::to_string(ngRanCell.size) +
                        " || ngRanCell.bits_unused != 4: " +
                        std::to_string(ngRanCell.bits_unused));
              }
              uint64_t nci = 0;
              for (auto i = 0, s = 32; i < 5; ++i, s -= 8) {
                nci |= ngRanCell.buf[i++] << s;
              }
              nci >>= ngRanCell.bits_unused;
              auto const& cellIdBitCnt = 36 - lmf_cfg.gnb_id_bits_count;
              auto const& gnbId        = nci >> cellIdBitCnt;

              if (this->gnb.count(gnbId) == 0) {
                static auto constexpr d1 = [](auto const& v) constexpr {
                  return v & 0xf;
                };
                static auto constexpr d2 = [](auto const& v) constexpr {
                  return v >> 4;
                };
                auto const& pb  = plmnnIdentity.buf;
                auto const& mcc = (boost::format("%0d%0d%0d") % d1(pb[0]) %
                                   d2(pb[0]) % d1(pb[1]))
                                      .str();
                auto const& mncd3 = d2(pb[1]);
                auto const& mnc2  = (mncd3 == 0xf);
                auto const& mnc =
                    mnc2 ? (boost::format("%0d%0d") % d1(pb[2]) % d2(pb[2]))
                               .str() :
                           (boost::format("%0d%0d%0d") % d1(pb[2]) % d2(pb[2]) %
                            mncd3)
                               .str();
                if (mcc.size() > 3) {
                  throwHttpError(
                      "trp information response", "invalid mcc: "s + mcc);
                }
                if (mnc.size() > (mnc2 ? 2 : 3)) {
                  throwHttpError(
                      "trp information response", "invalid mnc: "s + mnc);
                }

                PlmnId plmnId;
                plmnId.setMcc(mcc);
                plmnId.setMnc(mnc);

                auto const& gnbValue = (boost::format("%x") % gnbId).str();
                GNbId gNbId;
                gNbId.setGNBValue(gnbValue);
                gNbId.setBitLength(lmf_cfg.gnb_id_bits_count);

                GlobalRanNodeId globalRanNodeId;
                globalRanNodeId.setPlmnId(plmnId);
                globalRanNodeId.setGNbId(gNbId);

                Logger::lmf_app().info(
                    "trp information: adding gnb with id: " +
                    std::to_string(gnbId) + " mcc: '" + mcc + "' mnc: '" + mnc +
                    ":");

                if (auto const& [iter, inserted] =
                        this->gnb.try_emplace(gnbId, globalRanNodeId);
                    !inserted) {
                  throwHttpError(
                      "trp information response", "gnbId: "s +
                                                      std::to_string(gnbId) +
                                                      " already inserted"s);
                }
              }
              if (this->gnb.at(gnbId).trp.count(trpId) == 0) {
                Logger::lmf_app().info(
                    "trp information: adding to gnbId: " +
                    std::to_string(gnbId) + " trpId: " + std::to_string(trpId));
                Trp trp;  // TODO set Geographical Coordiantes, ...
                if (auto const& [iter, inserted] =
                        this->gnb.at(gnbId).trp.try_emplace(trpId, trp);
                    !inserted) {
                  throwHttpError(
                      "trp information response", "trpId: "s +
                                                      std::to_string(trpId) +
                                                      " already inserted"s);
                }
              } else {
                throwHttpError(
                    "trp information response",
                    "gnb_id: " + std::to_string(gnbId) +
                        "trp_id: " + std::to_string(trpId) + " not unique");
              }
            } else {
              throwHttpError(
                  "trp information response",
                  "TRPInformationItem_PR_nG_RAN_CGI not present, but: "s +
                      std::to_string(ngRanCgi->nG_RANcell.present));
            }
          }
        }
      }
    }
  }
  ASN_STRUCT_FREE(asn_DEF_NRPPA_PDU, nrppa);
  this->cv_gnb.notify_one();
}

bool lmf_app::handle_non_ue_n2info_nrppa_notification(NRPPA_PDU_t* nrppa) {
  auto const& nrppaTxnId = getNrppaTxnId(nrppa);
  auto const& supi       = this->extract_nrppaTxnId2Supi(nrppaTxnId);

  return this->handle_n2info_nrppa_notification(supi, nrppa);
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
    // don't forget tId handling ctx->nrppa_tId.erase(tId);
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
  // with multiple gnb expect multiple trp infos, erase after timeout
  if (procedureCode != ProcedureCode_id_tRPInformationExchange) {
    ctx->nrppa_tId.erase(tId);          // not for incomming/initiating!
    this->nrppa_tid_gen.free_uid(tId);  // for reuse
  }

  if (nrppa->present == NRPPA_PDU_PR_unsuccessfulOutcome) {
    auto const& unsuccessfulOutcome = getPR(
        nrppa->choice.unsuccessfulOutcome, *nrppa,
        NRPPA_PDU_PR_unsuccessfulOutcome);
    checkPC(unsuccessfulOutcome, procedureCode);
    switch (procedureCode) {
      case ProcedureCode_id_tRPInformationExchange: {
        std::scoped_lock lk{this->cv_m_gnb};

        struct trpInfoErr err {};
        auto const& value                 = unsuccessfulOutcome->value;
        auto const& trpInformationFailure = getPR(
            value.choice.TRPInformationFailure, value,
            UnsuccessfulOutcome__value_PR_TRPInformationFailure);

        for (auto const& trpInformationFailureIe :
             trpInformationFailure.protocolIEs) {
          switch (trpInformationFailureIe->id) {
            case ProtocolIE_ID_id_Cause: {
              auto const& value = trpInformationFailureIe->value;
              auto const& cause = getPR(
                  value.choice.Cause, value,
                  TRPInformationFailure_IEs__value_PR_Cause);
              switch (cause.present) {
                case Cause_PR_radioNetwork: {
                  auto const& radioNetwork = cause.choice.radioNetwork;
                  err.radio_network        = INTEGER_map_value2enum(
                      &asn_SPC_CauseRadioNetwork_specs_1, radioNetwork);
                } break;

                case Cause_PR_protocol: {
                  auto const& protocol = cause.choice.protocol;
                  err.protocol         = INTEGER_map_value2enum(
                      &asn_SPC_CauseProtocol_specs_1, protocol);
                } break;

                case Cause_PR_misc: {
                  auto const& misc = cause.choice.misc;
                  err.misc =
                      INTEGER_map_value2enum(&asn_SPC_CauseMisc_specs_1, misc);
                } break;

                default:
                  throwHttpError(
                      "trpInformationFailure",
                      "unknwon cause IE id: " + std::to_string(cause.present));
              }
            } break;

            case ProtocolIE_ID_id_CriticalityDiagnostics: {
            } break;

            default:
              throwHttpError(
                  "trpInformationFailure",
                  "unknwon IE id: " +
                      std::to_string(trpInformationFailureIe->id));
          }
        }
        Logger::lmf_app().error(
            "trp information failed: radio_network: %s protocol: %s misc: %s",
            err.radio_network ? err.radio_network->enum_name : "not set",
            err.protocol ? err.protocol->enum_name : "not set",
            err.misc ? err.misc->enum_name : "not set");
        this->trp_info_err.push_back(err);
        this->cv_gnb.notify_one();
        return true;
      }; break;

      case ProcedureCode_id_positioningInformationExchange: {
        auto const& value                         = unsuccessfulOutcome->value;
        auto const& positioningInformationFailure = getPR(
            value.choice.PositioningInformationFailure, value,
            UnsuccessfulOutcome__value_PR_PositioningInformationFailure);
        for (auto const& positioningInformationFailureIe :
             positioningInformationFailure.protocolIEs) {
          switch (positioningInformationFailureIe->id) {
            case ProtocolIE_ID_id_Cause: {
            } break;

            case ProtocolIE_ID_id_CriticalityDiagnostics: {
            } break;

            default:
              throwHttpError(
                  "positioningInformationFailure",
                  "unknwon IE id: " +
                      std::to_string(positioningInformationFailureIe->id));
          }
        }
      }; break;

      case ProcedureCode_id_positioningActivation: {
        auto const& value                        = unsuccessfulOutcome->value;
        auto const& positioningActivationFailure = getPR(
            value.choice.PositioningActivationFailure, value,
            UnsuccessfulOutcome__value_PR_PositioningActivationFailure);
      }; break;

      case ProcedureCode_id_Measurement: {
        auto const& value              = unsuccessfulOutcome->value;
        auto const& measurementFailure = getPR(
            value.choice.MeasurementFailure, value,
            UnsuccessfulOutcome__value_PR_MeasurementFailure);
      }; break;

      default:
        throwHttpError(
            "handle_nrppa_notification"s,
            "unsuccessfulOutcome: unhandled procedure code: %d"s +
                std::to_string(procedureCode));
    }
  }

  auto const& successfulOutcome = getPR(
      nrppa->choice.successfulOutcome, *nrppa, NRPPA_PDU_PR_successfulOutcome);
  checkPC(successfulOutcome, procedureCode);
  switch (procedureCode) {
    case ProcedureCode_id_tRPInformationExchange: {
      auto const& value                  = successfulOutcome->value;
      auto const& trpInformationResponse = getPR(
          value.choice.TRPInformationResponse, value,
          SuccessfulOutcome__value_PR_TRPInformationResponse);
      this->handle_trp_information_response(nrppa, tId, trpInformationResponse);
      return true;
    } break;

    case ProcedureCode_id_positioningInformationExchange: {
      auto const& value                          = successfulOutcome->value;
      auto const& positioningInformationResponse = getPR(
          value.choice.PositioningInformationResponse, value,
          SuccessfulOutcome__value_PR_PositioningInformationResponse);
      ctx->handle_positioning_information_response(
          nrppa, tId, positioningInformationResponse);
      return true;
    } break;

    case ProcedureCode_id_Measurement: {
      auto const& value               = successfulOutcome->value;
      auto const& measurementResponse = getPR(
          value.choice.MeasurementResponse, value,
          SuccessfulOutcome__value_PR_MeasurementResponse);
      ctx->handle_measurement_response(nrppa, tId, measurementResponse);
      return true;
    } break;

    case ProcedureCode_id_positioningActivation: {
      auto const& value                         = successfulOutcome->value;
      auto const& positioningActivationResponse = getPR(
          value.choice.PositioningActivationResponse, value,
          SuccessfulOutcome__value_PR_PositioningActivationResponse);
      ctx->handle_positioning_activation_response(
          nrppa, tId, positioningActivationResponse);
      return true;
    } break;

    default:
      throwHttpError(
          "handle_nrppa_notification"s,
          "successfulOutcome: unhandled procedure code: %d"s +
              std::to_string(procedureCode));
  }

  auto titel  = "n2info nrppa notifiaction pdu error"s;
  auto detail = "unhandled nrppa  pdu: " + std::to_string(nrppa->present);
  throwHttpError(titel, detail);

  return false;
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
  // xer_fprint(stdout, &asn_DEF_NRPPA_PDU, nrppa);
  Logger::lmf_server().debug("asn_decode ok, consumed: %d", rc.consumed);

  return nrppa;
}

void oai::lmf::app::lmf_app::insert_nrppaTxnId2supi(
    NRPPATransactionID_t const& nrppaTxnId, std::string const& supi) {
  std::unique_lock lock{this->m_nrppaTxnId2supi};
  auto const& [iter, insered] =
      this->nrppaTxnId2supi.try_emplace(nrppaTxnId, supi);
  if (!insered) {
    throwHttpError(
        "insert_nrppaTxnId2supi",
        "nrppa id "s + std::to_string(nrppaTxnId) + " reuse"s);
  }
}

std::string oai::lmf::app::lmf_app::extract_nrppaTxnId2Supi(
    NRPPATransactionID_t const& nrppaTxnId) {
  std::unique_lock lock{this->m_nrppaTxnId2supi};
  auto const& nh = this->nrppaTxnId2supi.extract(nrppaTxnId);
  if (nh.empty()) {
    throwHttpError(
        "extract_nrppaTxnId2Supi",
        "unknown nrppa txn id:"s + std::to_string(nrppaTxnId));
  }
  return nh.mapped();
}