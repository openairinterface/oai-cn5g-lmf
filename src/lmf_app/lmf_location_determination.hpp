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

#ifndef FILE_LMF_LOCATION_DETERMINATION_SEEN
#define FILE_LMF_LOCATION_DETERMINATION_SEEN

#include <future>
#include <map>
#include <tuple>
#include <set>

#include <nlohmann/json.hpp>

#include <pistache/http.h>
#include <pistache/router.h>

#define ASN_DISABLE_OER_SUPPORT
#include "NRPPA-PDU.h"
#include "NRPPATransactionID.h"
#include "PositioningInformationResponse.h"
#include "MeasurementResponse.h"
#include "PositioningActivationResponse.h"
#include "SRSConfiguration.h"
#include "ProcedureCode.h"
#include "Measurement-ID.h"
#include "TRPInformationResponse.h"

#include "GlobalRanNodeId.h"
#include "TRP-ID.h"

class LocationDetermination {
 public:
  LocationDetermination(std::string supi) : supi{supi} {}

  std::promise<std::pair<NRPPA_PDU_t*, PositioningActivationResponse_t const&>>
      positioning_activation_response;
  void positioning_activation_request(NRPPATransactionID_t const& tId);
  void handle_positioning_activation_response(
      NRPPA_PDU_t* nrppaPdu, NRPPATransactionID_t const& tId,
      PositioningActivationResponse_t const& positioningActivationResponse);

  std::promise<std::tuple<
      NRPPA_PDU_t*, PositioningInformationResponse_t const&,
      SRSConfiguration_t const&>>
      positioning_information_response;
  void positioning_information_request(NRPPATransactionID_t const& nrppa_tId);
  void handle_positioning_information_response(
      NRPPA_PDU_t* nrppaPdu, NRPPATransactionID_t const& tId,
      PositioningInformationResponse_t const& positioningInformationResponse);

  //   std::vector<
  //       std::promise<std::pair<NRPPA_PDU_t*, MeasurementResponse_t const&>>>
  //       resps;
  std::promise<std::pair<NRPPA_PDU_t*, MeasurementResponse_t const&>>
      measurement_response;
  std::future<std::pair<NRPPA_PDU_t*, MeasurementResponse_t const&>>
  measurement_request(
      NRPPATransactionID_t const& tId, Measurement_ID_t const& mId,
      std::vector<oai::lmf_server::model::GlobalRanNodeId> const&
          globalRanNodeList,
      std::set<TRP_ID_t> const& trpIds,
      SRSConfiguration_t const& srsConfiguration);
  void handle_measurement_response(
      NRPPA_PDU_t* nrppaPdu, NRPPATransactionID_t const& tId,
      MeasurementResponse_t const& measurementResponse);

  bool n1_n2_message_transfer(
      NRPPA_PDU_t* nrppaPdu, NRPPATransactionID_t const& txnId,
      ProcedureCode_t const& procedureCode);
  bool non_ue_n2_message_transfer(
      NRPPA_PDU_t* nrppaPdu, NRPPATransactionID_t const& txnId,
      ProcedureCode_t const& procedureCode,
      std::vector<oai::lmf_server::model::GlobalRanNodeId> const& grnidl,
      SRSConfiguration_t* const srsConfigurationBorrowed = nullptr);

  // mapping between nrppa transaction and transaction type
  // TODO: use individual reponse object as value not ResposeType
  //       to have more than one measurement at same time
  std::map<NRPPATransactionID_t, ProcedureCode_t> nrppa_tId;

 private:
  std::string supi;
};

#endif  // FILE_LMF_LOCATION_DETERMINATION_SEEN
