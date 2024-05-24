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

#ifndef FILE_LMF_CLIENT_HPP_SEEN
#define FILE_LMF_CLIENT_HPP_SEEN

#include <map>
#include <thread>
#include <string>
using namespace std::string_literals;

#include <curl/curl.h>
#include <pistache/http.h>

#include "lmf_config.hpp"
#include "logger.hpp"

namespace oai::lmf::app {

class lmf_client {
 private:
 public:
  lmf_client();
  virtual ~lmf_client();

  lmf_client(lmf_client const&) = delete;

  void curl_http_client(
      std::string remoteUri, std::string method, std::string msgBody,
      std::string& response, bool is_multipart);
};

void throwHttpError(
    std::string const& title, std::string const& detail,
    std::string const& instance = ""s,
    Pistache::Http::Code const& code =
        Pistache::Http::Code::Internal_Server_Error);

}  // namespace oai::lmf::app

extern oai::lmf::app::lmf_client* lmf_client_inst;

#endif /* FILE_LMF_CLIENT_HPP_SEEN */
