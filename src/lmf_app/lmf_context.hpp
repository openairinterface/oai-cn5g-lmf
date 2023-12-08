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

#ifndef FILE_LMF_CONTEXT_SEEN
#define FILE_LMF_CONTEXT_SEEN

#include <future>

#include <nlohmann/json.hpp>

#include <pistache/http.h>
#include <pistache/router.h>

#include "NRPPA-PDU.h"

#include "InputData.h"

class LMFContext {
 public:
  LMFContext(std::string supi) : supi{supi} {}

  void finish();
  std::promise<nlohmann::json> promise;

  void determine_location(
      const oai::lmf_server::model::InputData& inputData,
      nlohmann::json& json_data, Pistache::Http::Code& code);

  bool n1_n2_transfer(
      NRPPA_PDU_t* nrppaPdu, nlohmann::json& json_data,
      Pistache::Http::Code& code);

 private:
  std::string supi;
};

#endif
