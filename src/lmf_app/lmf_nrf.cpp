/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "lmf_nrf.hpp"

#include <nlohmann/json.hpp>

#include "lmf.h"
#include "lmf_config.hpp"
#include "logger.hpp"

using namespace oai::lmf::config;
using namespace oai::lmf::app;

extern lmf_config lmf_cfg;
extern std::shared_ptr<oai::sba::http_client> http_client_inst;

//------------------------------------------------------------------------------
lmf_nrf::lmf_nrf(const std::shared_ptr<lmf_event>& ev)
    : oai::sba::nf_service(ev, http_client_inst) {
  generate_lmf_profile();
}

//------------------------------------------------------------------------------
void lmf_nrf::generate_lmf_profile() {
  lmf_nf_profile.set_nf_instance_id(nf_instance_id);
  lmf_nf_profile.set_nf_instance_name("OAI-LMF");
  lmf_nf_profile.set_nf_type("LMF");
  lmf_nf_profile.set_nf_status("REGISTERED");
  lmf_nf_profile.set_nf_heartBeat_timer(50);
  lmf_nf_profile.set_nf_priority(1);
  lmf_nf_profile.set_nf_capacity(100);
  lmf_nf_profile.add_nf_ipv4_addresses(lmf_cfg.sbi.addr4);

  lmf_info_t lmf_info_item;
  lmf_info_item.lmfId = nf_instance_id;
  lmf_info_item.servingClientTypes.push_back(
      externalClientType_e2str.at(ExternalClientType_t::VALUE_ADDED_SERVICES));
  lmf_info_item.servingAccessTypes.push_back(
      accessType_e2str.at(AccessType_t::_3GPP_ACCESS));
  lmf_info_item.servingAnNodeTypes.push_back(
      anNodeType_e2str.at(AnNodeType_t::GNB));
  lmf_info_item.servingRatTypes.push_back(ratType_e2str.at(RatType_t::NR));
  lmf_nf_profile.set_lmf_info(lmf_info_item);

  lmf_nf_profile.display();
}

//------------------------------------------------------------------------------
bool lmf_nrf::register_to_nrf() {
  nlohmann::json json_data = {};
  lmf_nf_profile.to_json(json_data);
  Logger::lmf_nrf().debug("Sending NF registration request");
  return oai::sba::nf_service::register_to_nrf(lmf_cfg.nrf_addr, json_data);
}

//------------------------------------------------------------------------------
bool lmf_nrf::deregister_to_nrf() {
  Logger::lmf_nrf().info("Sending NF deregistration request");
  return oai::sba::nf_service::deregister_to_nrf();
}

//------------------------------------------------------------------------------
std::string lmf_nrf::get_nf_instance_id() const {
  return nf_instance_id;
}

//------------------------------------------------------------------------------
bool lmf_nrf::nrf_registration_enabled() const {
  return lmf_cfg.register_nrf;
}

//------------------------------------------------------------------------------
uint64_t lmf_nrf::nrf_registration_retry_seconds() const {
  return NRF_REGISTRATION_RETRY_TIMER;
}

//------------------------------------------------------------------------------
void lmf_nrf::on_registration_outcome(
    bool success, const oai::sba::response& resp) {
  if (success) {
    Logger::lmf_nrf().debug("Registered to NRF (status %d)", resp.status_code);
    start_event_nf_heartbeat(HEART_BEAT_TIMER);
    stop_nrf_registration_retry();
  } else {
    Logger::lmf_nrf().info("NF registration failed, try again");
    start_nrf_registration_retry();
  }
}
