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

/*! \file lmf_http2-server.cpp
 \brief
 \author  Tien-Thinh NGUYEN
 \company Eurecom
 \date 2020
 \email: tien-thinh.nguyen@eurecom.fr
 */

#include "lmf-http2-server.h"
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>
#include <regex>
#include <nlohmann/json.hpp>
#include <string>
#include "string.hpp"

#include "logger.hpp"
#include "lmf_config.hpp"
#include "3gpp_29.500.h"
#include "mime_parser.hpp"

using namespace nghttp2::asio_http2;
using namespace nghttp2::asio_http2::server;
//using namespace oai::lmf_server::model;
using namespace config;

extern lmf_config lmf_cfg;

//------------------------------------------------------------------------------
void lmf_http2_server::start() {
  boost::system::error_code ec;

  Logger::lmf_server().info("HTTP2 server started");

  // Default API
  /* TODO: Confirm base uri */
  server.handle(
      NLMF_AUTH_BASE + lmf_cfg.sbi_api_version,
      [&](const request& request, const response& response) {
        request.on_data([&](const uint8_t* data, std::size_t len) {
          std::string msg((char*) data, len);
          try {
            std::vector<std::string> split_result;
            boost::split(
                split_result, request.uri().path, boost::is_any_of("/"));
            if (request.method().compare("POST") == 0 && len > 0) {
              /*AuthenticationInfo authenticationInfo;
              nlohmann::json::parse(msg.c_str()).get_to(authenticationInfo);
              this->ue_authentications_post_handler(
                  authenticationInfo, response);*/
            }
          } catch (std::exception& e) {
            Logger::lmf_server().warn(
                "Invalid request (error: %s)!", e.what());
            response.write_head(
                http_status_code_e::HTTP_STATUS_CODE_400_BAD_REQUEST);
            response.end();
            return;
          }
        });
      });

  if (server.listen_and_serve(ec, m_address, std::to_string(m_port))) {
    std::cerr << "HTTP Server error: " << ec.message() << std::endl;
  }
}