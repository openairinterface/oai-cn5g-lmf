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

#ifndef FILE_LMF_APP_HPP_SEEN
#define FILE_LMF_APP_HPP_SEEN

#include <shared_mutex>
#include <string>
#include <map>
#include <boost/range/combine.hpp>

#include <pistache/http.h>

#include "uint_generator.hpp"

#include "lmf.h"
#include "lmf_event.hpp"
#include "lmf_context.hpp"
#include "lmf_n1_n2_message_subscription.hpp"

#include "ProblemDetails.h"
#include "InputData.h"

#include "NRPPATransactionID.h"

#include "lpp-ie-headers.hpp"

namespace oai::lmf::app {

class lmf_app {
 public:
  explicit lmf_app(const std::string& config_file, lmf_event& ev);
  lmf_app(lmf_app const&) = delete;
  void operator=(lmf_app const&) = delete;

  virtual ~lmf_app();

  void handle_determine_location(
      const oai::lmf_server::model::InputData& inputData,
      nlohmann::json& json_data, Pistache::Http::Code& code);

  bool handle_n2info_nrppa_notification(
      std::string supi, NRPPA_PDU_t* nrppa,
      oai::lmf_server::model::ProblemDetails& problem_details,
      uint8_t& http_code);

  bool is_supi_2_context(const std::string& supi) const;
  std::shared_ptr<LMFContext> create_lmf_context(const std::string& supi);
  std::shared_ptr<LMFContext> supi_2_context(const std::string& supi) const;
  void set_supi_2_context(
      const std::string& supi, const std::shared_ptr<LMFContext>& lc);
  void del_supi_2_context(const std::string& supi);

  std::shared_ptr<N1N2MessageSubscription> create_n1n2subscription(
      const std::string& supi);
  void release_n1n2subscription(const std::string& supi);

 private:
  std::map<std::string, std::shared_ptr<LMFContext>> supi2ctx;
  mutable std::shared_mutex m_supi2ctx;

  std::map<std::string, std::shared_ptr<N1N2MessageSubscription>> supi2n1n2subs;
  mutable std::shared_mutex m_supi2n1n2subs;

  lmf_event& event_sub;

  bool _is_supi_2_context(const std::string& supi) const;

  asn_encode_to_new_buffer_result_t build_trp_information_request_nrppa_pdu();

  util::uint_generator<NRPPATransactionID_t> nrppa_tid_gen;
};
}  // namespace oai::lmf::app

extern oai::lmf::app::lmf_app* lmf_app_inst;

#endif /* FILE_LMF_APP_HPP_SEEN */
