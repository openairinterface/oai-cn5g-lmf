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

#include "N2InfoNotifyApi.h"

#include "Helpers.h"
#include "lmf_config.hpp"

extern config::lmf_config lmf_cfg;

namespace oai {
namespace lmf_server {
namespace api {

// using namespace oai::lmf_server::helpers;
using namespace oai::lmf_server::model;

N2InfoNotifyApi::N2InfoNotifyApi(std::shared_ptr<Pistache::Rest::Router> rtr) {
  router = rtr;
}

void N2InfoNotifyApi::init() {
  setupRoutes();
}

void N2InfoNotifyApi::setupRoutes() {
  using namespace Pistache::Rest;

  Routes::Post(
      *router,
      base + lmf_cfg.sbi_api_version +
          "/position-information-response/callback/:ueContextId",
      Routes::bind(&N2InfoNotifyApi::notify_n2info_handler, this));

  // Default handler, called when a route is not found
  router->addCustomHandler(
      Routes::bind(&N2InfoNotifyApi::notify_n2info_default_handler, this));
}

void N2InfoNotifyApi::notify_n2info_handler(
    const Pistache::Rest::Request& request,
    Pistache::Http::ResponseWriter response) {
  // Get SUPI
  auto ueContextId = request.param(":ueContextId").as<std::string>();
  // Getting the body param
  N2InformationNotification n2InformationNotification;

  try {
    nlohmann::json::parse(request.body()).get_to(n2InformationNotification);
    this->receive_n2info_notification(
        ueContextId, n2InformationNotification, response);
  } catch (nlohmann::detail::exception& e) {
    // send a 400 error
    response.send(Pistache::Http::Code::Bad_Request, e.what());
    return;
  } catch (Pistache::Http::HttpError& e) {
    response.send(static_cast<Pistache::Http::Code>(e.code()), e.what());
    return;
  } catch (std::exception& e) {
    // send a 500 error
    response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
    return;
  }
}

void N2InfoNotifyApi::notify_n2info_default_handler(
    const Pistache::Rest::Request&, Pistache::Http::ResponseWriter response) {
  response.send(
      Pistache::Http::Code::Not_Found, "The requested method does not exist");
}

}  // namespace api
}  // namespace lmf_server
}  // namespace oai
