/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this
 * file except in compliance with the License. You may obtain a copy of the
 * License at
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

#ifndef FILE_LMF_CAUSE_ERROR_HPP_SEEN
#define FILE_LMF_CAUSE_ERROR_HPP_SEEN

#include "logger.hpp"

#include "Cause.h"

namespace oai::lmf::app {

class CauseError {
public:
  void parse(Cause_t const& cause)  {
    switch (cause.present) {
    case Cause_PR_radioNetwork: {
        auto const& radioNetwork = cause.choice.radioNetwork;
        this->radio_network        = INTEGER_map_value2enum(
            &asn_SPC_CauseRadioNetwork_specs_1, radioNetwork);
    } break;

    case Cause_PR_protocol: {
        auto const& protocol = cause.choice.protocol;
        this->protocol         = INTEGER_map_value2enum(
            &asn_SPC_CauseProtocol_specs_1, protocol);
    } break;

    case Cause_PR_misc: {
        auto const& misc = cause.choice.misc;
        this->misc =
            INTEGER_map_value2enum(&asn_SPC_CauseMisc_specs_1, misc);
    } break;

    default:
        Logger::lmf_app().warn(
            "trpInformationFailure: unknwon cause IE id: %d",
            cause.present);
    }
  }
  
  void log() {
    Logger::lmf_app().error(
        "trp information failed: radio_network: %s protocol: %s misc: %s",
        this->radio_network ? this->radio_network->enum_name : "not set",
        this->protocol ? this->protocol->enum_name : "not set",
        this->misc ? this->misc->enum_name : "not set");
  }

  asn_INTEGER_enum_map_t const *radio_network = nullptr, *protocol = nullptr, *misc = nullptr;
};

} // oai::lmf::app

#endif // FILE_LMF_CAUSE_ERROR_HPP_SEEN