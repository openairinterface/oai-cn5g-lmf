/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this
 *file except in compliance with the License. You may obtain a copy of the
 *License at
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

#include "lmf_config.hpp"

#include "string.hpp"
#include <iostream>
#include <libconfig.h++>

#include "fqdn.hpp"
#include "if.hpp"
#include "logger.hpp"

#include "string.hpp"

#include <arpa/inet.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include "common_defs.h"

using namespace libconfig;

namespace oai::lmf::config {

//------------------------------------------------------------------------------
lmf_config::lmf_config() : sbi() {
  use_fqdn_dns = false;
  use_http2    = false;
}

//------------------------------------------------------------------------------
lmf_config::~lmf_config() {}

//------------------------------------------------------------------------------
void lmf_config::display() {
  Logger::config().info(
      "==== OAI-CN5G %s v%s ====", PACKAGE_NAME, PACKAGE_VERSION);
  Logger::config().info("================= LMF =================");
  Logger::config().info("Configuration LMF:");
  Logger::config().info("- Instance ...............: %d", instance);
  Logger::config().info("- PID Dir ................: %s", pid_dir.c_str());
  Logger::config().info("- LMF Name ...............: %s", lmf_name.c_str());
  Logger::config().info(
      "- Log Level will be .......: %s",
      spdlog::level::to_string_view(log_level));
  Logger::config().info("- HTTP Threads ...........: %d", http_threads_count);
  Logger::config().info("- GNB ID bits count ......: %d", gnb_id_bits_count);
  Logger::config().info("- mum GNB ................: %d", num_gnb);
  Logger::config().info(
      "- TRP info wait time .....: %dms", trp_info_wait_ms.count());
  Logger::config().info(
      "- Positioning wait time ..: %dms", positioning_wait_ms.count());

  Logger::config().info("- SBI Networking:");
  Logger::config().info("    Iface ................: %s", sbi.if_name.c_str());
  Logger::config().info("    IPv4 Addr ............: %s", inet_ntoa(sbi.addr4));
  Logger::config().info("    HTTP1 Port ...........: %d", sbi.port);
  Logger::config().info("    HTTP2 Port............: %d", sbi_http2_port);
  Logger::config().info(
      "    API Version...........: %s", sbi_api_version.c_str());
  Logger::config().info("- Supported Features:");
  Logger::config().info(
      "    Register NRF ..........: %s", register_nrf ? "Yes" : "No");
  Logger::config().info(
      "    Use FQDN ..............: %s", use_fqdn_dns ? "Yes" : "No");
  Logger::config().info(
      "    Use HTTP2..............: %s", use_http2 ? "Yes" : "No");
  Logger::config().info(
      "    determine num gnb......: %s", determine_num_gnb ? "Yes" : "No");

  if (register_nrf) {
    Logger::config().info("- NRF:");
    Logger::config().info(
        "    IPv4 Addr ............: %s",
        inet_ntoa(*((struct in_addr*) &nrf_addr.ipv4_addr)));
    Logger::config().info("    Port .................: %lu  ", nrf_addr.port);
    Logger::config().info(
        "    API version ..........: %s", nrf_addr.api_version.c_str());
  }

  Logger::config().info("- AMF:");
  Logger::config().info(
      "    IPv4 Addr ............: %s",
      inet_ntoa(*((struct in_addr*) &amf_addr.ipv4_addr)));
  Logger::config().info("    Port .................: %lu  ", amf_addr.port);
  Logger::config().info(
      "    API version ..........: %s", amf_addr.api_version.c_str());
}

}  // namespace oai::lmf::config
