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

/*! \file lmf_profile.cpp
 \brief
 \author  Tien-Thinh NGUYEN
 \company Eurecom
 \date 2021
 \email: Tien-Thinh.Nguyen@eurecom.fr
 */

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include "lmf_profile.hpp"
#include "logger.hpp"
#include "string.hpp"

// using namespace lmf;
using namespace oai::lmf::app;

//------------------------------------------------------------------------------
void lmf_profile::set_nf_instance_id(const std::string& instance_id) {
  nf_instance_id = instance_id;
}

//------------------------------------------------------------------------------
void lmf_profile::get_nf_instance_id(std::string& instance_id) const {
  instance_id = nf_instance_id;
}

//------------------------------------------------------------------------------
std::string lmf_profile::get_nf_instance_id() const {
  return nf_instance_id;
}

//------------------------------------------------------------------------------
void lmf_profile::set_nf_instance_name(const std::string& instance_name) {
  nf_instance_name = instance_name;
}

//------------------------------------------------------------------------------
void lmf_profile::get_nf_instance_name(std::string& instance_name) const {
  instance_name = nf_instance_name;
}

//------------------------------------------------------------------------------
std::string lmf_profile::get_nf_instance_name() const {
  return nf_instance_name;
}

//------------------------------------------------------------------------------
void lmf_profile::set_nf_type(const std::string& type) {
  nf_type = type;
}

//------------------------------------------------------------------------------
std::string lmf_profile::get_nf_type() const {
  return nf_type;
}
//------------------------------------------------------------------------------
void lmf_profile::set_nf_status(const std::string& status) {
  nf_status = status;
}

//------------------------------------------------------------------------------
void lmf_profile::get_nf_status(std::string& status) const {
  status = nf_status;
}

//------------------------------------------------------------------------------
std::string lmf_profile::get_nf_status() const {
  return nf_status;
}

//------------------------------------------------------------------------------
void lmf_profile::set_nf_heartBeat_timer(const int32_t& timer) {
  heartBeat_timer = timer;
}

//------------------------------------------------------------------------------
void lmf_profile::get_nf_heartBeat_timer(int32_t& timer) const {
  timer = heartBeat_timer;
}

//------------------------------------------------------------------------------
int32_t lmf_profile::get_nf_heartBeat_timer() const {
  return heartBeat_timer;
}

//------------------------------------------------------------------------------
void lmf_profile::set_nf_priority(const uint16_t& p) {
  priority = p;
}

//------------------------------------------------------------------------------
void lmf_profile::get_nf_priority(uint16_t& p) const {
  p = priority;
}

//------------------------------------------------------------------------------
uint16_t lmf_profile::get_nf_priority() const {
  return priority;
}

//------------------------------------------------------------------------------
void lmf_profile::set_nf_capacity(const uint16_t& c) {
  capacity = c;
}

//------------------------------------------------------------------------------
void lmf_profile::get_nf_capacity(uint16_t& c) const {
  c = capacity;
}

//------------------------------------------------------------------------------
uint16_t lmf_profile::get_nf_capacity() const {
  return capacity;
}

//------------------------------------------------------------------------------
void lmf_profile::set_nf_snssais(const std::vector<snssai_t>& s) {
  snssais = s;
}

//------------------------------------------------------------------------------
void lmf_profile::get_nf_snssais(std::vector<snssai_t>& s) const {
  s = snssais;
}

//------------------------------------------------------------------------------
void lmf_profile::add_snssai(const snssai_t& s) {
  snssais.push_back(s);
}

//------------------------------------------------------------------------------
void lmf_profile::set_fqdn(const std::string& fqdN) {
  fqdn = fqdN;
}

//------------------------------------------------------------------------------
std::string lmf_profile::get_fqdn() const {
  return fqdn;
}

//------------------------------------------------------------------------------
void lmf_profile::set_nf_ipv4_addresses(const std::vector<struct in_addr>& a) {
  ipv4_addresses = a;
}

//------------------------------------------------------------------------------
void lmf_profile::add_nf_ipv4_addresses(const struct in_addr& a) {
  ipv4_addresses.push_back(a);
}
//------------------------------------------------------------------------------
void lmf_profile::get_nf_ipv4_addresses(std::vector<struct in_addr>& a) const {
  a = ipv4_addresses;
}

//------------------------------------------------------------------------------
void lmf_profile::set_lmf_info(const lmf_info_t& s) {
  lmf_info = s;
}

//------------------------------------------------------------------------------
void lmf_profile::get_lmf_info(lmf_info_t& s) const {
  s = lmf_info;
}

//------------------------------------------------------------------------------
void lmf_profile::display() const {
  Logger::lmf_app().debug("- NF instance info");
  Logger::lmf_app().debug("    Instance ID: %s", nf_instance_id.c_str());
  Logger::lmf_app().debug("    Instance name: %s", nf_instance_name.c_str());
  Logger::lmf_app().debug("    Instance type: %s", nf_type.c_str());
  Logger::lmf_app().debug("    Instance fqdn: %s", fqdn.c_str());
  Logger::lmf_app().debug("    Status: %s", nf_status.c_str());
  Logger::lmf_app().debug("    HeartBeat timer: %d", heartBeat_timer);
  Logger::lmf_app().debug("    Priority: %d", priority);
  Logger::lmf_app().debug("    Capacity: %d", capacity);
  // SNSSAIs
  if (snssais.size() > 0) {
    Logger::lmf_app().debug("    SNSSAI:");
  }
  for (auto s : snssais) {
    Logger::lmf_app().debug("        SST, SD: %d, %s", s.sST, s.sD.c_str());
  }

  // IPv4 Addresses
  if (ipv4_addresses.size() > 0) {
    Logger::lmf_app().debug("    IPv4 Addr:");
  }
  for (auto address : ipv4_addresses) {
    Logger::lmf_app().debug("        %s", inet_ntoa(address));
  }

  Logger::lmf_app().debug("\tLMF Info");
  Logger::lmf_app().debug("\t\tGroupId: %s", lmf_info.groupid);
  /* TODO: is this needed in lmf */
  for (auto supi : lmf_info.supi_ranges) {
    Logger::lmf_app().debug(
        "\t\t SupiRanges: Start - %s, End - %s, Pattern - %s",
        supi.supi_range.start, supi.supi_range.end, supi.supi_range.pattern);
  }
  for (auto route_ind : lmf_info.routing_indicators) {
    Logger::lmf_app().debug("\t\t Routing Indicators: %s", route_ind);
  }
}

//------------------------------------------------------------------------------
void lmf_profile::to_json(nlohmann::json& data) const {
  data["nfInstanceId"]   = nf_instance_id;
  data["nfInstanceName"] = nf_instance_name;
  data["nfType"]         = nf_type;
  data["nfStatus"]       = nf_status;
  data["heartBeatTimer"] = heartBeat_timer;
  // SNSSAIs
  data["sNssais"] = nlohmann::json::array();
  for (auto s : snssais) {
    nlohmann::json tmp = {};
    tmp["sst"]         = s.sST;
    tmp["sd"]          = s.sD;
    data["sNssais"].push_back(tmp);
  }
  data["fqdn"] = fqdn;
  // ipv4_addresses
  data["ipv4Addresses"] = nlohmann::json::array();
  for (auto address : ipv4_addresses) {
    nlohmann::json tmp = inet_ntoa(address);
    data["ipv4Addresses"].push_back(tmp);
  }

  data["priority"] = priority;
  data["capacity"] = capacity;

  // LMF Info
  data["lmfInfo"]["groupId"]           = lmf_info.groupid;
  data["lmfInfo"]["supiRanges"]        = nlohmann::json::array();
  data["lmfInfo"]["routingIndicators"] = nlohmann::json::array();
  for (auto supi : lmf_info.supi_ranges) {
    nlohmann::json tmp = {};
    tmp["start"]       = supi.supi_range.start;
    tmp["end"]         = supi.supi_range.end;
    tmp["pattern"]     = supi.supi_range.pattern;
    data["lmfInfo"]["supiRanges"].push_back(tmp);
  }
  for (auto route_ind : lmf_info.routing_indicators) {
    std::string tmp = route_ind;
    data["lmfInfo"]["routingIndicators"].push_back(route_ind);
  }

  Logger::lmf_app().debug("lmf profile to JSON:\n %s", data.dump().c_str());
}

//------------------------------------------------------------------------------
void lmf_profile::from_json(const nlohmann::json& data) {
  if (data.find("nfInstanceId") != data.end()) {
    nf_instance_id = data["nfInstanceId"].get<std::string>();
  }

  if (data.find("nfInstanceName") != data.end()) {
    nf_instance_name = data["nfInstanceName"].get<std::string>();
  }

  if (data.find("nfType") != data.end()) {
    nf_type = data["nfType"].get<std::string>();
  }

  if (data.find("nfStatus") != data.end()) {
    nf_status = data["nfStatus"].get<std::string>();
  }

  if (data.find("heartBeatTimer") != data.end()) {
    heartBeat_timer = data["heartBeatTimer"].get<int>();
  }
  // sNssais
  if (data.find("sNssais") != data.end()) {
    for (auto it : data["sNssais"]) {
      snssai_t s = {};
      s.sST      = it["sst"].get<int>();
      s.sD       = it["sd"].get<std::string>();
      snssais.push_back(s);
    }
  }

  if (data.find("ipv4Addresses") != data.end()) {
    nlohmann::json addresses = data["ipv4Addresses"];

    for (auto it : addresses) {
      struct in_addr addr4 = {};
      std::string address  = it.get<std::string>();
      unsigned char buf_in_addr[sizeof(struct in_addr)];
      if (inet_pton(AF_INET, util::trim(address).c_str(), buf_in_addr) == 1) {
        memcpy(&addr4, buf_in_addr, sizeof(struct in_addr));
      } else {
        Logger::lmf_app().warn(
            "Address conversion: Bad value %s", util::trim(address).c_str());
      }
      add_nf_ipv4_addresses(addr4);
    }
  }

  if (data.find("priority") != data.end()) {
    priority = data["priority"].get<int>();
  }

  if (data.find("capacity") != data.end()) {
    capacity = data["capacity"].get<int>();
  }

  // LMF info
  if (data.find("lmfInfo") != data.end()) {
    nlohmann::json info = data["lmfInfo"];
    if (info.find("groupId") != info.end()) {
      lmf_info.groupid = info["groupId"].get<std::string>();
    }
    if (info.find("routingIndicators") != info.end()) {
      nlohmann::json routing_indicators_list =
          data["lmfInfo"]["routingIndicators"];
      for (auto d : routing_indicators_list) {
        lmf_info.routing_indicators.push_back(d);
      }
    }
    if (info.find("supiRanges") != info.end()) {
      nlohmann::json supi_ranges = data["lmfInfo"]["supiRanges"];
      for (auto d : supi_ranges) {
        supi_range_lmf_info_item_t supi;
        supi.supi_range.start   = d["start"];
        supi.supi_range.end     = d["end"];
        supi.supi_range.pattern = d["pattern"];
        lmf_info.supi_ranges.push_back(supi);
      }
    }
  }
  display();
}

//------------------------------------------------------------------------------
void lmf_profile::handle_heartbeart_timeout(uint64_t ms) {
  Logger::lmf_app().info(
      "Handle heartbeart timeout profile %s, time %d", nf_instance_id.c_str(),
      ms);
  set_nf_status("SUSPENDED");
}
