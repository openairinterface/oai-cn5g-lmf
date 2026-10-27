/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FILE_LMF_PROFILE_HPP_SEEN
#define FILE_LMF_PROFILE_HPP_SEEN

#include "lmf.h"
#include "../common-src/3gpp/3gpp_29.510.h"
#include "../common-src/sba/nf_profile.hpp"

namespace oai::lmf::app {

class lmf_profile : public oai::sba::nf_profile {
 public:
  lmf_profile() = default;
  explicit lmf_profile(const std::string& id) : oai::sba::nf_profile(id) {}
  lmf_profile(lmf_profile const&)    = delete;
  void operator=(lmf_profile const&) = delete;
  ~lmf_profile() override            = default;

  void set_lmf_info(const lmf_info_t& info);
  void get_lmf_info(lmf_info_t& info) const;

  void display() override;
  void to_json(nlohmann::json& data) const override;
  void from_json(const nlohmann::json& data);
  void handle_heartbeart_timeout(uint64_t ms);

 private:
  lmf_info_t lmf_info;
};

}  // namespace oai::lmf::app

#endif
