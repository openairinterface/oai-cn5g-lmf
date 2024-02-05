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

#include "lmf-http2-server.h"
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>
#include <regex>
#include <nlohmann/json.hpp>
#include <pistache/http.h>
#include <string>
#include <filesystem>
#include "string.hpp"

#include "logger.hpp"
#include "lmf_config.hpp"
#include "3gpp_29.500.h"
#include "mime_parser.hpp"

#include "N2InformationNotification.h"
#include "ProblemDetails.h"

using namespace nghttp2::asio_http2;
using namespace nghttp2::asio_http2::server;
using namespace config;
using namespace oai::lmf_server;

extern lmf_config lmf_cfg;

//------------------------------------------------------------------------------
void lmf_http2_server::start() {
  boost::system::error_code ec;

  Logger::lmf_server().info("HTTP2 server started");

  // Default API
  /* TODO: Confirm base uri */
  server.handle(
      NLMF_BASE + lmf_cfg.sbi_api_version + NLMF_DETERMINE_LOCATION,
      [&](const request& request, const response& response) {
        auto requestBody = std::make_shared<std::stringstream>();
        request.on_data([requestBody, &request, &response, this](
                            const uint8_t* data, std::size_t len) {
          try {
            if (len > 0) {
              std::copy(
                  data, data + len,
                  std::ostream_iterator<uint8_t>(*requestBody));
            } else {
              auto const& msg = requestBody->str();
              requestBody->clear();
              if (msg.size() == 0 || request.method().compare("POST") != 0) {
                throw std::runtime_error("invalid request");
              }
              model::InputData inputData{nlohmann::json::parse(msg)};
              this->detemine_location_post_handler(inputData, response);
            }
          } catch (std::exception& e) {
            Logger::lmf_server().warn("Invalid request (error: %s)!", e.what());
            response.write_head(
                http_status_code_e::HTTP_STATUS_CODE_400_BAD_REQUEST);
            response.end();
            return;
          }
        });
        response.on_close([](uint32_t cause) {
          //          Logger::lmf_server().debug("<<-- determine location
          //          on_close with cause: " + std::to_string(cause));
        });
      });

  // /nlmf-n2info-notify/v1/nrppa/callback/imsi-208950000000131
  server.handle(
      NLMF_NOTIFY_BASE + lmf_cfg.sbi_api_version + NLMF_NOTIFY_NRPPA_CALLBACK,
      [&](const request& request, const response& response) {
        auto requestBody = std::make_shared<std::stringstream>();
        request.on_data([requestBody, &request, &response, this](
                            const uint8_t* data, std::size_t len) {
          try {
            if (len > 0) {
              std::copy(
                  data, data + len,
                  std::ostream_iterator<uint8_t>(*requestBody));
            } else {
              auto const& msg = requestBody->str();
              requestBody->clear();
              std::filesystem::path path{request.uri().path};
              std::vector<std::string> split_result{path.begin(), path.end()};
              if (msg.size() == 0 || request.method().compare("POST") != 0 ||
                  split_result.size() != 6) {
                throw std::invalid_argument("invalid request");
              }
              auto ueContextId = split_result.at(5);
              mime_parser sp;
              if (!sp.parse(msg)) {
                throw std::invalid_argument{"can not parse multipart"};
              }
              std::vector<mime_part> parts;
              sp.get_mime_parts(parts);
              if (parts.size() != 2) {
                throw std::invalid_argument{"expect two parts"};
              }
              this->n2info_nrppa_notification_post_handler(
                  ueContextId, parts, response);
            }
          } catch (std::exception& e) {
            Logger::lmf_server().warn("Invalid request (error: %s)!", e.what());
            response.write_head(
                http_status_code_e::HTTP_STATUS_CODE_400_BAD_REQUEST);
            response.end();
            return;
          }
        });
        response.on_close([](uint32_t cause) {
          //          Logger::lmf_server().debug("<<-- notify on_close with
          //          cause: " + std::to_string(cause));
        });
      });

  // multi threaded is needed to handle incomming AMF notifications during
  // processing determine locaiton
  server.num_threads(m_num_threads);
  if (server.listen_and_serve(ec, m_address, std::to_string(m_port))) {
    std::cerr << "HTTP Server error: " << ec.message() << std::endl;
  }
}

void lmf_http2_server::n2info_nrppa_notification_post_handler(
    const std::string& ueContextId, std::vector<mime_part>& parts,
    const response& response) {
  model::N2InformationNotification n2InformationNotification{
      nlohmann::json::parse(parts.at(0).body)};
  // TODO: handle subscrription id
  auto const& n2NotifySubscriptionId =
      n2InformationNotification.getN2NotifySubscriptionId();
  // TODO: handle lcs corrlation id
  n2InformationNotification.getLcsCorrelationId();

  Logger::lmf_server().debug("SUPI %s", ueContextId);

  auto nrppa = lmf_app::parse_n2_info_container_nrppa(
      n2InformationNotification, parts.at(1));
  header_map h;
  unsigned code = HTTP_STATUS_CODE_204_NO_CONTENT;
  model::ProblemDetails problemDetails;
  std::string reason;
  try {
    if (!m_lmf_app->handle_n2info_nrppa_notification(ueContextId, nrppa)) {
      N1N2MessageSubscription::unsubscribe(ueContextId, n2NotifySubscriptionId);
    }
  } catch (nlohmann::detail::exception& e) {
    problemDetails.setDetail(e.what());
    code = HTTP_STATUS_CODE_400_BAD_REQUEST;
  } catch (Pistache::Http::HttpError& e) {
    code = e.code();
    problemDetails.setDetail(e.what());
    reason = e.what();
  } catch (std::exception& e) {
    problemDetails.setDetail(e.what());
    code = HTTP_STATUS_CODE_500_INTERNAL_SERVER_ERROR;
  }
  if (code != HTTP_STATUS_CODE_204_NO_CONTENT) {
    problemDetails.setTitle("handle_n2info_nrppa_notification failed");
    Logger::lmf_server().error(
        problemDetails.getTitle() + ": " + problemDetails.getDetail());
    h.insert(std::make_pair<std::string, header_value>(
        "Content-Type", {"application/json", false}));
    if (reason.empty()) {
      reason = nlohmann::json(problemDetails).dump();
    }
  }
  response.write_head(code, h);
  response.end(reason);
}

void lmf_http2_server::detemine_location_post_handler(
    const oai::lmf_server::model::InputData& inputData,
    const response& response) {
  Logger::lmf_server().info("Received determine_location_post Request");

  nlohmann::json locationData_json = {};
  Pistache::Http::Code code        = {};
  header_map h;

  m_lmf_app->handle_determine_location(inputData, locationData_json, code);

  if (code == Pistache::Http::Code::Ok) {
    h.insert(std::make_pair<std::string, header_value>(
        "Content-Type", {"application/json", false}));
    response.write_head(HTTP_STATUS_CODE_200_OK, h);
    response.end(locationData_json.dump());
  } else {
    response.write_head(static_cast<uint32_t>(code), h);
    response.end();
  }
}
