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

#ifndef FILE_LMF_NRF_SEEN
#define FILE_LMF_NRF_SEEN

#include <map>
#include <thread>

#include <curl/curl.h>

#include "PatchItem.h"
#include "lmf_config.hpp"
#include "lmf_event.hpp"
#include "lmf_profile.hpp"
#include "logger.hpp"

// using namespace oai::lmf_server::model;

namespace oai {
namespace lmf {
namespace app {

class lmf_nrf {
 private:
 public:
  lmf_profile lmf_nf_profile;   // LMF profile
  std::string lmf_instance_id;  // LMF instance id
  // timer_id_t timer_lmf_heartbeat;

  lmf_nrf(lmf_event& ev);
  lmf_nrf(lmf_nrf const&) = delete;
  void operator=(lmf_nrf const&) = delete;

  void generate_uuid();
  /*
   * Start event nf heartbeat procedure
   * @param [void]
   * @return void
   */
  void start_event_nf_heartbeat(std::string& remoteURI);
  /*
   * Trigger NF heartbeat procedure
   * @param [void]
   * @return void
   */
  void trigger_nf_heartbeat_procedure(uint64_t ms);
  /*
   * Generate a LMF profile for this instance
   * @param [void]
   * @return void
   */
  void generate_lmf_profile(
      lmf_profile& lmf_nf_profile, std::string& lmf_instance_id);

  /*
   * Trigger NF instance registration to NRF
   * @param [void]
   * @return void
   */
  void register_to_nrf();
  /*
   * Get lmf API Root
   * @param [std::string& ] api_root: lmf's API Root
   * @return void
   */
  void get_lmf_api_root(std::string& api_root);

 private:
  lmf_event& m_event_sub;
  bs2::connection task_connection;
};
}  // namespace app
}  // namespace lmf
}  // namespace oai
#endif /* FILE_LMF_NRF_SEEN */
