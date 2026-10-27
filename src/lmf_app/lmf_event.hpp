/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FILE_LMF_EVENT_HPP_SEEN
#define FILE_LMF_EVENT_HPP_SEEN

#include "lmf.h"
#include "../common-src/sba/nf_event.hpp"

namespace oai::lmf::app {
class lmf_event : public oai::sba::nf_event {
 public:
  lmf_event()                      = default;
  lmf_event(lmf_event const&)      = delete;
  ~lmf_event() override            = default;
  void operator=(lmf_event const&) = delete;

  static lmf_event& get_instance() {
    static lmf_event instance;
    return instance;
  }

  // class register/handle event
  friend class lmf_app;
  friend class lmf_nrf;
};
}  // namespace oai::lmf::app
#endif
