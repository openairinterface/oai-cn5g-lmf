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

namespace config {

//------------------------------------------------------------------------------
lmf_config::lmf_config() : sbi() {
  use_fqdn_dns = false;
  use_http2    = false;
}

//------------------------------------------------------------------------------
lmf_config::~lmf_config() {}

//------------------------------------------------------------------------------
int lmf_config::load(const std::string& config_file) {
  Logger::config().debug(
      "\nLoad LMF system configuration file(%s)", config_file.c_str());
  Config cfg;
  unsigned char buf_in6_addr[sizeof(struct in6_addr)];

  try {
    cfg.readFile(config_file.c_str());
  } catch (const FileIOException& fioex) {
    Logger::config().error(
        "I/O error while reading file %s - %s", config_file.c_str(),
        fioex.what());
    throw;
  } catch (const ParseException& pex) {
    Logger::config().error(
        "Parse error at %s:%d - %s", pex.getFile(), pex.getLine(),
        pex.getError());
    throw;
  }
  const Setting& root = cfg.getRoot();

  try {
    const Setting& lmf_cfg = root[LMF_CONFIG_STRING_LMF_CONFIG];
  } catch (const SettingNotFoundException& nfex) {
    Logger::config().error("%s : %s", nfex.what(), nfex.getPath());
    return RETURNerror;
  }
  const Setting& lmf_cfg = root[LMF_CONFIG_STRING_LMF_CONFIG];
  try {
    this->instance = lmf_cfg.lookup(LMF_CONFIG_STRING_INSTANCE_ID);
  } catch (const SettingNotFoundException& nfex) {
    Logger::config().error(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }

  try {
    this->pid_dir =
        lmf_cfg.lookup(LMF_CONFIG_STRING_PID_DIRECTORY).operator std::string();
  } catch (const SettingNotFoundException& nfex) {
    Logger::config().error(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }
  try {
    this->lmf_name =
        lmf_cfg.lookup(LMF_CONFIG_STRING_LMF_NAME).operator std::string();
  } catch (const SettingNotFoundException& nfex) {
    Logger::config().error(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }
  // Log Level
  try {
    this->log_level = spdlog::level::from_str(
        lmf_cfg.lookup(LMF_CONFIG_STRING_LOG_LEVEL).operator std::string());
  } catch (const SettingNotFoundException& nfex) {
    Logger::config().error(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }
  try {
    this->http_threads_count =
        lmf_cfg.lookup(LMF_CONFIG_STRING_HTTP_THREADS_COUNT);
  } catch (const SettingNotFoundException& nfex) {
    Logger::config().error(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }
  try {
    this->gnb_id_bits_count =
        lmf_cfg.lookup(LMF_CONFIG_STRING_GNB_ID_BITS_COUNT);
  } catch (const SettingNotFoundException& nfex) {
    Logger::config().error(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }
  try {
    this->num_gnb = lmf_cfg.lookup(LMF_CONFIG_STRING_NUM_GNB);
  } catch (const SettingNotFoundException& nfex) {
    Logger::config().error(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }
  try {
    this->trp_info_wait_ms = std::chrono::milliseconds(
        lmf_cfg.lookup(LMF_CONFIG_STRING_TRP_INFO_WAIT_MS)
            .
            operator unsigned int());
  } catch (const SettingNotFoundException& nfex) {
    Logger::config().error(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }
  try {
    this->positioning_wait_ms = std::chrono::milliseconds(
        lmf_cfg.lookup(LMF_CONFIG_STRING_POSITIONING_WAIT_MS)
            .
            operator unsigned int());
  } catch (const SettingNotFoundException& nfex) {
    Logger::config().error(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }
  try {
    this->measurement_wait_ms = std::chrono::milliseconds(
        lmf_cfg.lookup(LMF_CONFIG_STRING_MEASUREMENT_WAIT_MS)
            .
            operator unsigned int());
  } catch (const SettingNotFoundException& nfex) {
    Logger::config().error(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }

  // LMF SBI interface
  try {
    const Setting& new_if_cfg = lmf_cfg[LMF_CONFIG_STRING_INTERFACES];

    const Setting& sbi_cfg = new_if_cfg[LMF_CONFIG_STRING_INTERFACE_SBI];
    load_interface(sbi_cfg, sbi);
    // HTTP2 port
    if (!(sbi_cfg.lookupValue(
            LMF_CONFIG_STRING_SBI_HTTP2_PORT, sbi_http2_port))) {
      Logger::lmf_app().error(LMF_CONFIG_STRING_SBI_HTTP2_PORT "failed");
      throw(LMF_CONFIG_STRING_SBI_HTTP2_PORT " failed");
    }

    // API Version
    if (!(sbi_cfg.lookupValue(
            LMF_CONFIG_STRING_API_VERSION, sbi_api_version))) {
      Logger::lmf_app().error(LMF_CONFIG_STRING_API_VERSION " failed");
      throw(LMF_CONFIG_STRING_API_VERSION " failed");
    }

  } catch (const SettingNotFoundException& nfex) {
    Logger::config().error(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
    return RETURNerror;
  }

  // Support features
  try {
    const Setting& support_features =
        lmf_cfg[LMF_CONFIG_STRING_SUPPORT_FEATURES];
    std::string opt = {};

    support_features.lookupValue(
        LMF_CONFIG_STRING_SUPPORT_FEATURES_USE_FQDN_DNS, opt);
    if (boost::iequals(opt, "yes")) {
      use_fqdn_dns = true;
    } else {
      use_fqdn_dns = false;
    }

    support_features.lookupValue(
        LMF_CONFIG_STRING_SUPPORT_FEATURES_USE_HTTP2, opt);
    if (boost::iequals(opt, "yes")) {
      use_http2 = true;
    } else {
      use_http2 = false;
    }

    support_features.lookupValue(
        LMF_CONFIG_STRING_SUPPORTED_FEATURES_REGISTER_NRF, opt);
    if (boost::iequals(opt, "yes")) {
      register_nrf = true;
    } else {
      register_nrf = false;
    }

    support_features.lookupValue(
        LMF_CONFIG_STRING_SUPPORTED_FEATURES_REQUEST_TRP_INFO, opt);
    if (boost::iequals(opt, "yes")) {
      request_trp_info = true;
    } else {
      request_trp_info = false;
    }

    support_features.lookupValue(
        LMF_CONFIG_STRING_SUPPORTED_FEATURES_DETERMINE_NUM_GNB, opt);
    if (boost::iequals(opt, "yes")) {
      determine_num_gnb = true;
    } else {
      determine_num_gnb = false;
    }
  } catch (const SettingNotFoundException& nfex) {
    Logger::lmf_app().error(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
    return RETURNerror;
  }

  try {
    std::string astring = {};

    const Setting& amf_cfg       = lmf_cfg[LMF_CONFIG_STRING_AMF];
    struct in_addr amf_ipv4_addr = {};
    unsigned int amf_port        = 0;
    std::string amf_api_version  = {};

    if (!use_fqdn_dns) {
      amf_cfg.lookupValue(LMF_CONFIG_STRING_AMF_IPV4_ADDRESS, astring);
      IPV4_STR_ADDR_TO_INADDR(
          util::trim(astring).c_str(), amf_ipv4_addr,
          "BAD IPv4 ADDRESS FORMAT FOR AMF !");
      amf_addr.ipv4_addr = amf_ipv4_addr;
      if (!(amf_cfg.lookupValue(LMF_CONFIG_STRING_AMF_PORT, amf_port))) {
        Logger::lmf_app().error(LMF_CONFIG_STRING_AMF_PORT "failed");
        throw(LMF_CONFIG_STRING_AMF_PORT "failed");
      }
      amf_addr.port = amf_port;

      if (!(amf_cfg.lookupValue(
              LMF_CONFIG_STRING_API_VERSION, amf_api_version))) {
        Logger::lmf_app().error(LMF_CONFIG_STRING_API_VERSION "failed");
        throw(LMF_CONFIG_STRING_API_VERSION "failed");
      }
      amf_addr.api_version = amf_api_version;

    } else {
      amf_cfg.lookupValue(LMF_CONFIG_STRING_FQDN_DNS, astring);
      uint8_t addr_type   = {0};
      std::string address = {};
      fqdn::resolve(astring, address, amf_port, addr_type);
      if (addr_type != 0) {  // IPv6
        // TODO:
        throw("DO NOT SUPPORT IPV6 ADDR FOR AMF!");
      } else {  // IPv4
        IPV4_STR_ADDR_TO_INADDR(
            util::trim(address).c_str(), amf_ipv4_addr,
            "BAD IPv4 ADDRESS FORMAT FOR NRF !");
        amf_addr.ipv4_addr = amf_ipv4_addr;
        // amf_addr.port               = amf_port;
        // We hardcode amf port from config for the moment
        if (!(amf_cfg.lookupValue(LMF_CONFIG_STRING_AMF_PORT, amf_port))) {
          Logger::lmf_app().error(LMF_CONFIG_STRING_AMF_PORT "failed");
          throw(LMF_CONFIG_STRING_AMF_PORT "failed");
        }
        amf_addr.port               = amf_port;
        std::string amf_api_version = {};
        if (!(amf_cfg.lookupValue(
                LMF_CONFIG_STRING_API_VERSION, amf_api_version))) {
          Logger::lmf_app().error(LMF_CONFIG_STRING_API_VERSION "failed");
          throw(LMF_CONFIG_STRING_API_VERSION "failed");
        }
        amf_addr.api_version =
            amf_api_version;  // TODO: to get API version from DNS
        amf_addr.fqdn = astring;
      }
    }

  } catch (const SettingNotFoundException& nfex) {
    Logger::lmf_app().error("%s : %s", nfex.what(), nfex.getPath());
    return RETURNerror;
  }

  // NRF
  if (register_nrf) {
    try {
      std::string astring = {};

      const Setting& nrf_cfg       = lmf_cfg[LMF_CONFIG_STRING_NRF];
      struct in_addr nrf_ipv4_addr = {};
      unsigned int nrf_port        = 0;
      std::string nrf_api_version  = {};

      if (!use_fqdn_dns) {
        nrf_cfg.lookupValue(LMF_CONFIG_STRING_NRF_IPV4_ADDRESS, astring);
        IPV4_STR_ADDR_TO_INADDR(
            util::trim(astring).c_str(), nrf_ipv4_addr,
            "BAD IPv4 ADDRESS FORMAT FOR NRF !");
        nrf_addr.ipv4_addr = nrf_ipv4_addr;
        if (!(nrf_cfg.lookupValue(LMF_CONFIG_STRING_NRF_PORT, nrf_port))) {
          Logger::lmf_app().error(LMF_CONFIG_STRING_NRF_PORT "failed");
          throw(LMF_CONFIG_STRING_NRF_PORT "failed");
        }
        nrf_addr.port = nrf_port;

        if (!(nrf_cfg.lookupValue(
                LMF_CONFIG_STRING_API_VERSION, nrf_api_version))) {
          Logger::lmf_app().error(LMF_CONFIG_STRING_API_VERSION "failed");
          throw(LMF_CONFIG_STRING_API_VERSION "failed");
        }
        nrf_addr.api_version = nrf_api_version;

      } else {
        nrf_cfg.lookupValue(LMF_CONFIG_STRING_FQDN_DNS, astring);
        uint8_t addr_type   = {0};
        std::string address = {};
        fqdn::resolve(astring, address, nrf_port, addr_type);
        if (addr_type != 0) {  // IPv6
          // TODO:
          throw("DO NOT SUPPORT IPV6 ADDR FOR NRF!");
        } else {  // IPv4
          IPV4_STR_ADDR_TO_INADDR(
              util::trim(address).c_str(), nrf_ipv4_addr,
              "BAD IPv4 ADDRESS FORMAT FOR NRF !");
          nrf_addr.ipv4_addr = nrf_ipv4_addr;
          // nrf_addr.port        = nrf_port;
          nrf_addr.api_version = "v1";  // TODO: to get API version from DNS
          nrf_addr.fqdn        = astring;
          // We hardcode nrf port from config for the moment
          if (!(nrf_cfg.lookupValue(LMF_CONFIG_STRING_NRF_PORT, nrf_port))) {
            Logger::lmf_app().error(LMF_CONFIG_STRING_NRF_PORT "failed");
            throw(LMF_CONFIG_STRING_NRF_PORT "failed");
          }
          nrf_addr.port = nrf_port;
        }
      }
    } catch (const SettingNotFoundException& nfex) {
      Logger::lmf_app().error("%s : %s", nfex.what(), nfex.getPath());
      return RETURNerror;
    }
  }
  return RETURNok;
}

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

  if (use_fqdn_dns)
    Logger::config().info(
        "    FQDN .................: %s", nrf_addr.fqdn.c_str());
}

//------------------------------------------------------------------------------
int lmf_config::load_interface(
    const libconfig::Setting& if_cfg, interface_cfg_t& cfg) {
  if_cfg.lookupValue(LMF_CONFIG_STRING_INTERFACE_NAME, cfg.if_name);
  util::trim(cfg.if_name);
  if (not boost::iequals(cfg.if_name, "none")) {
    std::string address = {};
    if_cfg.lookupValue(LMF_CONFIG_STRING_IPV4_ADDRESS, address);
    util::trim(address);
    if (boost::iequals(address, "read")) {
      if (get_inet_addr_infos_from_iface(
              cfg.if_name, cfg.addr4, cfg.network4, cfg.mtu)) {
        Logger::config().error(
            "Could not read %s network interface configuration", cfg.if_name);
        return RETURNerror;
      }
    } else {
      std::vector<std::string> words;
      boost::split(
          words, address, boost::is_any_of("/"), boost::token_compress_on);
      if (words.size() != 2) {
        Logger::config().error(
            "Bad value " LMF_CONFIG_STRING_IPV4_ADDRESS " = %s in config file",
            address.c_str());
        return RETURNerror;
      }
      unsigned char buf_in_addr[sizeof(struct in6_addr)];  // you never know...
      if (inet_pton(AF_INET, util::trim(words.at(0)).c_str(), buf_in_addr) ==
          1) {
        memcpy(&cfg.addr4, buf_in_addr, sizeof(struct in_addr));
      } else {
        Logger::config().error(
            "In conversion: Bad value " LMF_CONFIG_STRING_IPV4_ADDRESS
            " = %s in config file",
            util::trim(words.at(0)).c_str());
        return RETURNerror;
      }
      cfg.network4.s_addr = htons(
          ntohs(cfg.addr4.s_addr) &
          0xFFFFFFFF << (32 - std::stoi(util::trim(words.at(1)))));
    }
    if_cfg.lookupValue(LMF_CONFIG_STRING_PORT, cfg.port);
  }
  return RETURNok;
}

}  // namespace config
