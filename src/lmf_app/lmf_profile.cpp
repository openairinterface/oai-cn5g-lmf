/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "lmf_profile.hpp"

#include <arpa/inet.h>

#include "logger.hpp"
#include "string.hpp"

using namespace oai::lmf::app;

//------------------------------------------------------------------------------
void lmf_profile::set_lmf_info(const lmf_info_t& info) {
  lmf_info = info;
}

//------------------------------------------------------------------------------
void lmf_profile::get_lmf_info(lmf_info_t& info) const {
  info = lmf_info;
}

//------------------------------------------------------------------------------
void lmf_profile::display() {
  oai::sba::nf_profile::display();

  Logger::lmf_app().debug("\tLMF Info");
  Logger::lmf_app().debug("\t\t LmfId: %s", lmf_info.lmfId.c_str());

  Logger::lmf_app().debug("\t\t ServingClientTypes: ");
  for (const auto& client_type : lmf_info.servingClientTypes) {
    Logger::lmf_app().debug("\t\t\t %s", client_type.c_str());
  }

  Logger::lmf_app().debug("\t\t ServingAccessTypes: ");
  for (const auto& access_type : lmf_info.servingAccessTypes) {
    Logger::lmf_app().debug("\t\t\t %s", access_type.c_str());
  }

  Logger::lmf_app().debug("\t\t ServingAnNodeTypes: ");
  for (const auto& node_type : lmf_info.servingAnNodeTypes) {
    Logger::lmf_app().debug("\t\t\t %s", node_type.c_str());
  }

  Logger::lmf_app().debug("\t\t ServingRatTypes: ");
  for (const auto& rat_type : lmf_info.servingRatTypes) {
    Logger::lmf_app().debug("\t\t\t %s", rat_type.c_str());
  }
}

//------------------------------------------------------------------------------
void lmf_profile::to_json(nlohmann::json& data) const {
  oai::sba::nf_profile::to_json(data);

  auto& info                 = data["lmfInfo"];
  info["lmfId"]              = lmf_info.lmfId;
  info["servingClientTypes"] = lmf_info.servingClientTypes;
  info["servingAccessTypes"] = lmf_info.servingAccessTypes;
  info["servingAnNodeTypes"] = lmf_info.servingAnNodeTypes;
  info["servingRatTypes"]    = lmf_info.servingRatTypes;

  Logger::lmf_app().debug("lmf profile to JSON:\n %s", data.dump().c_str());
}

//------------------------------------------------------------------------------
void lmf_profile::from_json(const nlohmann::json& data) {
  if (data.contains("nfInstanceId"))
    set_nf_instance_id(data.at("nfInstanceId").get<std::string>());
  if (data.contains("nfInstanceName"))
    set_nf_instance_name(data.at("nfInstanceName").get<std::string>());
  if (data.contains("nfType"))
    set_nf_type(data.at("nfType").get<std::string>());
  if (data.contains("nfStatus"))
    set_nf_status(data.at("nfStatus").get<std::string>());
  if (data.contains("heartBeatTimer"))
    set_nf_heartBeat_timer(data.at("heartBeatTimer").get<int32_t>());
  if (data.contains("fqdn")) set_fqdn(data.at("fqdn").get<std::string>());
  if (data.contains("priority"))
    set_nf_priority(data.at("priority").get<uint16_t>());
  if (data.contains("capacity"))
    set_nf_capacity(data.at("capacity").get<uint16_t>());

  if (data.contains("ipv4Addresses")) {
    for (const auto& value : data.at("ipv4Addresses")) {
      struct in_addr address{};
      auto text = value.get<std::string>();
      oai::utils::trim(text);
      if (inet_pton(AF_INET, text.c_str(), &address) == 1) {
        add_nf_ipv4_addresses(address);
      } else {
        Logger::lmf_app().warn(
            "Address conversion: Bad value %s", text.c_str());
      }
    }
  }

  if (data.contains("lmfInfo")) {
    const auto& info = data.at("lmfInfo");
    if (info.contains("lmfId"))
      lmf_info.lmfId = info.at("lmfId").get<std::string>();
    if (info.contains("servingClientTypes"))
      lmf_info.servingClientTypes =
          info.at("servingClientTypes").get<std::vector<std::string>>();
    if (info.contains("servingAccessTypes"))
      lmf_info.servingAccessTypes =
          info.at("servingAccessTypes").get<std::vector<std::string>>();
    if (info.contains("servingAnNodeTypes"))
      lmf_info.servingAnNodeTypes =
          info.at("servingAnNodeTypes").get<std::vector<std::string>>();
    if (info.contains("servingRatTypes"))
      lmf_info.servingRatTypes =
          info.at("servingRatTypes").get<std::vector<std::string>>();
  }
}

//------------------------------------------------------------------------------
void lmf_profile::handle_heartbeart_timeout(uint64_t ms) {
  Logger::lmf_app().info(
      "Handle heartbeart timeout profile %s, time %d",
      get_nf_instance_id().c_str(), ms);
  set_nf_status("SUSPENDED");
}
