/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FILE_LMF_NRF_SEEN
#define FILE_LMF_NRF_SEEN

#include "lmf_event.hpp"
#include "lmf_profile.hpp"
#include "../common-src/sba/nf_service.hpp"

namespace oai::lmf::app {

class lmf_nrf : public oai::sba::nf_service {
 public:
  explicit lmf_nrf(const std::shared_ptr<lmf_event>& ev);
  lmf_nrf(lmf_nrf const&)        = delete;
  ~lmf_nrf() override            = default;
  void operator=(lmf_nrf const&) = delete;

  bool register_to_nrf();
  bool deregister_to_nrf();
  std::string get_nf_instance_id() const;

  void generate_lmf_profile();

  lmf_profile lmf_nf_profile;

 protected:
  bool nrf_registration_enabled() const override;
  uint64_t nrf_registration_retry_seconds() const override;
  void on_registration_outcome(
      bool success, const oai::sba::response& resp) override;
};

}  // namespace oai::lmf::app

extern oai::lmf::app::lmf_nrf* lmf_nrf_inst;

#endif /* FILE_LMF_NRF_SEEN */
