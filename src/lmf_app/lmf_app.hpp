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

/*! \file lmf_app.hpp
 \brief
 \author  Tien-Thinh NGUYEN
 \company Eurecom
 \date 2021
 \email:
 */

#ifndef FILE_LMF_APP_HPP_SEEN
#define FILE_LMF_APP_HPP_SEEN
#include "lmf_event.hpp"

#include "lmf.h"
#include "InputData.h"
#include "lpp-ie-headers.hpp"
#include <map>
#include <pistache/http.h>
#include <shared_mutex>
#include <string>

namespace oai {
namespace lmf {
namespace app {

//using namespace oai::lmf_server::model;

// class lmf_config;
class lmf_app {
 public:
  explicit lmf_app(const std::string& config_file, lmf_event& ev);
  lmf_app(lmf_app const&) = delete;
  void operator=(lmf_app const&) = delete;

  virtual ~lmf_app();

  void handle_determine_location(const oai::lmf_server::model::InputData& inputData, nlohmann::json& json_data, Pistache::Http::Code& code, uint8_t http_version = 1);

 private:
  lmf_event& event_sub;
  void build_request_location_lpp_pdu(LPP_Message_t *lppMsg);
};
}  // namespace app
}  // namespace lmf
}  // namespace oai
#include "lmf_config.hpp"

#endif /* FILE_LMF_APP_HPP_SEEN */
