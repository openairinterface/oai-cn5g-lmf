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
#include "mime_parser.hpp"

#include "uint_generator.hpp"

#include "lmf.h"
#include "lmf_event.hpp"
#include "lmf_location_determination.hpp"
#include "lmf_n1_n2_message_subscription.hpp"
#include "lmf_non_ue_n2_message_subscription.hpp"

#include "ProblemDetails.h"
#include "InputData.h"
#include "N2InformationNotification.h"

#include "NRPPATransactionID.h"
#include "Measurement-ID.h"
#include "TRP-ID.h"

namespace oai::lmf::app {

class Gnb {
 public:
  Gnb(oai::lmf_server::model::GlobalRanNodeId const& ncgi) : ncgi{ncgi} {}
  const oai::lmf_server::model::GlobalRanNodeId ncgi;
  std::set<TRP_ID_t> trpIds;  // TODO: replace with class TRP
};

class lmf_app {
 public:
  explicit lmf_app(const std::string& config_file, lmf_event& ev);
  lmf_app(lmf_app const&) = delete;
  void operator=(lmf_app const&) = delete;

  virtual ~lmf_app();

  void handle_determine_location(
      const oai::lmf_server::model::InputData& inputData,
      nlohmann::json& json_data, Pistache::Http::Code& code);

  bool handle_non_ue_n2info_nrppa_notification(NRPPA_PDU_t* nrppa);

  bool handle_n2info_nrppa_notification(std::string supi, NRPPA_PDU_t* nrppa);

  bool is_supi_2_context(const std::string& supi) const;
  std::shared_ptr<LocationDetermination> create_lmf_context(
      const std::string& supi);
  std::shared_ptr<LocationDetermination> supi_2_context(
      const std::string& supi) const;
  void set_supi_2_context(
      const std::string& supi,
      const std::shared_ptr<LocationDetermination>& lc);
  void del_supi_2_context(const std::string& supi);

  void create_n1n2subscription(const std::string& supi);
  void release_n1n2subscription(const std::string& supi);

  static NRPPA_PDU_t* parse_n2_info_container_nrppa(
      oai::lmf_server::model::N2InformationNotification const&
          n2InformationNotification,
      mime_part const& nrppa_part);

  // for non-ue that actually refers to ue
  std::map<NRPPATransactionID_t, std::string> supiByNrppaTxnId;

 private:
  std::map<std::string, std::shared_ptr<LocationDetermination>> supi2ctx;
  mutable std::shared_mutex m_supi2ctx;

  std::map<std::string, N1N2MessageSubscription> supi2n1n2subs;
  mutable std::shared_mutex m_supi2n1n2subs;

  std::unique_ptr<NonUeN2MessageSubscription> nonUeN2MessageSubscription;

  lmf_event& event_sub;

  bool _is_supi_2_context(const std::string& supi) const;

  template<auto t>
  using val      = std::integral_constant<std::decay_t<decltype(t)>, t>;
  using gc_c_ptr = std::unique_ptr<void, val<std::free>>;
  std::pair<asn_encode_to_new_buffer_result_t, gc_c_ptr>
  build_trp_information_request_nrppa_pdu();

  util::uint_generator<NRPPATransactionID_t, 0, 32767> nrppa_tid_gen;
  NRPPATransactionID_t nrppa_tid_trp_information;

  util::uint_generator<Measurement_ID_t, 1, 65536> measurement_id_gen;

  // NG_RAN_CGI_t / NG_RANCell_t / NRCellIdentifier_t /
  // std::map<GNB_ID, std::vector<TRP_ID_t>> trps = {{1, {1}}};

  // globalRanNodeList
  using GnbId = uint32_t;
  std::map<GnbId, Gnb> gnb;
};
}  // namespace oai::lmf::app

extern oai::lmf::app::lmf_app* lmf_app_inst;

#endif /* FILE_LMF_APP_HPP_SEEN */
