/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FILE_LMF_GNB_SEEN
#define FILE_LMF_GNB_SEEN

#include "lmf_trp.hpp"

#include "GlobalRanNodeId.h"

#include "TRP-ID.h"

namespace oai::lmf::app {
using GnbId = uint64_t;

class Gnb {
 public:
  Gnb(GnbId const& id, oai::_3gpp::model::GlobalRanNodeId const& ncgi)
      : id{id}, ncgi{ncgi} {}
  const GnbId id;
  const oai::_3gpp::model::GlobalRanNodeId ncgi;
  std::map<TRP_ID_t, Trp> trp;
};

}  // namespace oai::lmf::app

#endif  // FILE_LMF_GNB_SEEN
