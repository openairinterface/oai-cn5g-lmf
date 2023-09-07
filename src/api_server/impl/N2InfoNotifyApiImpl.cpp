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

#include "logger.hpp"
#include "N2InfoNotifyApiImpl.h"
#include "conversions.hpp"

namespace oai {
namespace lmf_server {
namespace api {

using namespace oai::lmf_server::model;

N2InfoNotifyApiImpl::N2InfoNotifyApiImpl(
    std::shared_ptr<Pistache::Rest::Router> rtr,
    oai::lmf::app::lmf_app* lmf_app_inst)
    : N2InfoNotifyApi(rtr), m_lmf_app(lmf_app_inst) {}

void N2InfoNotifyApiImpl::receive_n2info_notification(
    const std::string& ueContextId,
    const N2InformationNotification& n2InformationNotification,
    Pistache::Http::ResponseWriter& response) {
  Logger::lmf_server().debug("Receive an N2Info Notify, handling...");
}

}  // namespace api
}  // namespace lmf_server
}  // namespace oai
