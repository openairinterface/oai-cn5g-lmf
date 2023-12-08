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

#include <nlohmann/json.hpp>

#include <pistache/http.h>
#include <pistache/router.h>
#define ASN_DISABLE_OER_SUPPORT
#include "NRPPA-PDU.h"
#include "NRPPATransactionID.h"
#include "PositioningInformationResponse.h"
#include "MeasurementResponse.h"

#include "InputData.h"

enum class ResponseType { PositionInformation, Measurement };

class LocationDetermination {
 public:
  LocationDetermination(std::string supi) : supi{supi} {}

  std::promise<std::pair<NRPPA_PDU_t*, PositioningInformationResponse_t const&>>
      position_information_response;
  std::promise<std::pair<NRPPA_PDU_t*, MeasurementResponse_t const&>>
      measurement_response;

  void position_information_request(NRPPATransactionID_t const& nrppa_tId);

  void handle_position_information_response(
      NRPPA_PDU_t* nrppaPdu, NRPPATransactionID_t const& tId,
      PositioningInformationResponse_t const& positioningInformationResponse);
  void handle_measurement_response(
      NRPPA_PDU_t* nrppaPdu, NRPPATransactionID_t const& tId,
      MeasurementResponse_t const& measurementResponse);

  void measurement_request(NRPPATransactionID_t const& tId);

  bool n1_n2_message_transfer(NRPPA_PDU_t* nrppaPdu);

  // mapping between nrppa transaction and transaction type
  // TODO: use individual reponse object as value not ResposeType
  //       to have more than one measurement at same time
  std::map<NRPPATransactionID_t, ResponseType> nrppa_tId;

 private:
  std::string supi;
};

#endif  // FILE_LMF_LOCATION_DETERMINATION_SEEN
