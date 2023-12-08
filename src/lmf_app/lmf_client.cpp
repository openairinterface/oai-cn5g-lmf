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

#include "lmf_client.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <pistache/http.h>
#include <pistache/mime.h>
#include <stdexcept>

#include "ProblemDetails.h"

#include "lmf.h"
#include "logger.hpp"

using namespace Pistache::Http;
using namespace Pistache::Http::Mime;
using namespace oai::lmf::app;
using namespace config;
using json = nlohmann::json;

extern lmf_client* lmf_client_inst;
extern lmf_config lmf_cfg;

//------------------------------------------------------------------------------
// To read content of the response from NF
static std::size_t callback(
    const char* in, std::size_t size, std::size_t num, std::string* out) {
  const std::size_t totalBytes(size * num);
  out->append(in, totalBytes);
  return totalBytes;
}

//------------------------------------------------------------------------------
lmf_client::lmf_client() {}

//------------------------------------------------------------------------------
lmf_client::~lmf_client() {
  Logger::lmf_app().debug("Delete LMF Client instance...");
}

//------------------------------------------------------------------------------
void lmf_client::curl_http_client(
    std::string remoteUri, std::string method, std::string msgBody,
    std::string& response, bool is_multipart) {
  Logger::lmf_app().info("Send HTTP message with body %s", msgBody.c_str());

  uint32_t str_len = msgBody.length();
  char* body_data  = (char*) malloc(str_len + 1);
  memset(body_data, 0, str_len + 1);
  memcpy((void*) body_data, (void*) msgBody.c_str(), str_len);

  curl_global_init(CURL_GLOBAL_ALL);
  CURL* curl = curl_easy_init();

  uint8_t http_version = 1;
  if (lmf_cfg.use_http2) http_version = 2;

  if (curl) {
    CURLcode res               = {};
    struct curl_slist* headers = nullptr;
    if ((method.compare("POST") == 0) or (method.compare("PUT") == 0) or
        (method.compare("PATCH") == 0)) {
      if (is_multipart) {
        std::string content_type =
            "Content-type: multipart/related; boundary=" +
            std::string(CURL_MIME_BOUNDARY);
        headers = curl_slist_append(headers, content_type.c_str());
      } else {
        headers = curl_slist_append(headers, "Content-Type: application/json");
      }
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    curl_easy_setopt(curl, CURLOPT_URL, remoteUri.c_str());
    if (method.compare("POST") == 0)
      curl_easy_setopt(curl, CURLOPT_HTTPPOST, 1);
    else if (method.compare("PUT") == 0)
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    else if (method.compare("DELETE") == 0)
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    else if (method.compare("PATCH") == 0)
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    else
      curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);

    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, CURL_TIMEOUT_MS);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1);
    curl_easy_setopt(curl, CURLOPT_INTERFACE, lmf_cfg.sbi.if_name.c_str());

    if (http_version == 2) {
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      // we use a self-signed test server, skip verification during debugging
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      curl_easy_setopt(
          curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    }

    // Response information.
    long httpCode = {0};
    std::unique_ptr<std::string> httpData(new std::string());
    std::unique_ptr<std::string> httpHeaderData(new std::string());

    // Hook up data handling function.
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, httpHeaderData.get());

    if ((method.compare("POST") == 0) or (method.compare("PUT") == 0) or
        (method.compare("PATCH") == 0)) {
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, msgBody.length());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_data);
    }
    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    // Process the response
    response            = *httpData.get();
    bool is_response_ok = true;
    Logger::lmf_app().info("Get response with HTTP code (%d)", httpCode);

    if (httpCode == 0) {
      Logger::lmf_app().info(
          "Cannot get response when calling %s", remoteUri.c_str());
      // free curl before returning
      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      if (response.size() < 1) {
        response = "Cannot get response when calling " + remoteUri;
      }
      return;
    }

    nlohmann::json response_data = {};

    if (httpCode != HTTP_RESPONSE_CODE_OK &&
        httpCode != HTTP_RESPONSE_CODE_CREATED &&
        httpCode != HTTP_RESPONSE_CODE_NO_CONTENT) {
      is_response_ok = false;
      if (response.size() < 1) {
        Logger::lmf_app().info("There's no content in the response");
        // TODO: send context response error
        response = "failed with code " + std::to_string(httpCode) +
                   " and empty resonse";
        return;
      }
      Logger::lmf_app().warn("Receive response with HTTP code %d", httpCode);
      return;
    }

    if (!is_response_ok) {
      try {
        response_data = nlohmann::json::parse(response);
      } catch (nlohmann::json::exception& e) {
        Logger::lmf_app().info("Could not get JSON content from the response");
        // Set the default Cause
        response_data["error"]["cause"] = "504 Gateway Timeout";
      }

      Logger::lmf_app().info(
          "Get response with jsonData: %s", response.c_str());

      std::string cause = response_data["error"]["cause"];
      Logger::lmf_app().info("Call Network Function services failure");
      Logger::lmf_app().info("Cause value: %s", cause.c_str());
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }

  curl_global_cleanup();

  if (body_data) {
    free(body_data);
    body_data = NULL;
  }
  return;
}

void oai::lmf::app::throwHttpError(
    std::string const& title, std::string const& detail,
    Pistache::Http::Code const& code) {
  oai::lmf_server::model::ProblemDetails problemDetails;
  problemDetails.setTitle(title);
  problemDetails.setDetail(detail);
  Logger::lmf_server().error(
      problemDetails.getTitle() + ": " + problemDetails.getDetail());
  auto const& reason = nlohmann::json(problemDetails).dump();
  throw HttpError{code, reason};
}
