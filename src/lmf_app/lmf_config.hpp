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

#ifndef _LMF_CONFIG_H_
#define _LMF_CONFIG_H_

#include <arpa/inet.h>

#include <libconfig.h++>
#include <string>
#include <chrono>

#include "logger_base.hpp"

#define LMF_CONFIG_STRING_LMF_CONFIG "LMF"
#define LMF_CONFIG_STRING_PID_DIRECTORY "PID_DIRECTORY"
#define LMF_CONFIG_STRING_INSTANCE_ID "INSTANCE_ID"
#define LMF_CONFIG_STRING_LMF_NAME "LMF_NAME"
#define LMF_CONFIG_STRING_LOG_LEVEL "LOG_LEVEL"
#define LMF_CONFIG_STRING_HTTP_THREADS_COUNT "HTTP_THREADS_COUNT"
#define LMF_CONFIG_STRING_GNB_ID_BITS_COUNT "GNB_ID_BITS_COUNT"
#define LMF_CONFIG_STRING_NUM_GNB "NUM_GNB"
#define LMF_CONFIG_STRING_TRP_INFO_WAIT_MS "TRP_INFO_WAIT_MS"
#define LMF_CONFIG_STRING_POSITIONING_WAIT_MS "POSITIONING_WAIT_MS"
#define LMF_CONFIG_STRING_MEASUREMENT_WAIT_MS "MEASUREMENT_WAIT_MS"

#define LMF_CONFIG_STRING_INTERFACES "INTERFACES"
#define LMF_CONFIG_STRING_INTERFACE_SBI "SBI"
#define LMF_CONFIG_STRING_SBI_HTTP2_PORT "HTTP2_PORT"

#define LMF_CONFIG_STRING_INTERFACE_NAME "INTERFACE_NAME"
#define LMF_CONFIG_STRING_IPV4_ADDRESS "IPV4_ADDRESS"
#define LMF_CONFIG_STRING_PORT "PORT"
#define LMF_CONFIG_STRING_API_VERSION "API_VERSION"

#define LMF_CONFIG_STRING_AMF "AMF"
#define LMF_CONFIG_STRING_AMF_IPV4_ADDRESS "IPV4_ADDRESS"
#define LMF_CONFIG_STRING_AMF_PORT "PORT"

#define LMF_CONFIG_STRING_NRF "NRF"
#define LMF_CONFIG_STRING_NRF_IPV4_ADDRESS "IPV4_ADDRESS"
#define LMF_CONFIG_STRING_NRF_PORT "PORT"

#define LMF_CONFIG_STRING_SUPPORT_FEATURES "SUPPORT_FEATURES"
#define LMF_CONFIG_STRING_SUPPORT_FEATURES_USE_FQDN_DNS "USE_FQDN_DNS"
#define LMF_CONFIG_STRING_SUPPORT_FEATURES_USE_HTTP2 "USE_HTTP2"
#define LMF_CONFIG_STRING_SUPPORTED_FEATURES_REGISTER_NRF "REGISTER_NRF"
#define LMF_CONFIG_STRING_SUPPORTED_FEATURES_REQUEST_TRP_INFO "REQUEST_TRP_INFO"
#define LMF_CONFIG_STRING_SUPPORTED_FEATURES_DETERMINE_NUM_GNB                 \
  "DETERMINE_NUM_GNB"
#define LMF_CONFIG_STRING_FQDN_DNS "FQDN"

namespace config {

typedef struct interface_cfg_s {
  std::string if_name;
  struct in_addr addr4;
  struct in_addr network4;
  struct in6_addr addr6;
  unsigned int mtu;
  unsigned int port;
} interface_cfg_t;

class lmf_config {
 public:
  lmf_config();
  ~lmf_config();
  int load(const std::string& config_file);
  int load_interface(const libconfig::Setting& if_cfg, interface_cfg_t& cfg);
  void display();

  unsigned int instance               = 1;
  std::string pid_dir                 = "/var/run";
  std::string lmf_name                = "OAI_LMF";
  spdlog::level::level_enum log_level = spdlog::level::debug;
  unsigned http_threads_count         = 8;
  unsigned gnb_id_bits_count          = 28;
  bool determine_num_gnb              = false;
  unsigned num_gnb                    = 1;
  std::chrono::milliseconds trp_info_wait_ms{10000};
  std::chrono::milliseconds positioning_wait_ms{10000};
  std::chrono::milliseconds measurement_wait_ms{10000};

  interface_cfg_t sbi;
  unsigned int sbi_http2_port;
  std::string sbi_api_version;

  struct {
    struct in_addr ipv4_addr;
    unsigned int port;
    std::string api_version;
    std::string fqdn;
  } amf_addr;

  struct {
    struct in_addr ipv4_addr;
    unsigned int port;
    std::string api_version;
    std::string fqdn;
  } nrf_addr;

  bool register_nrf;
  bool request_trp_info;
  bool use_fqdn_dns;
  bool use_http2;
};

}  // namespace config

extern config::lmf_config lmf_cfg;

#endif
