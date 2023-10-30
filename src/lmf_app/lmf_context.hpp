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

#ifndef FILE_LMF_CONTEXT_SEEN
#define FILE_LMF_CONTEXT_SEEN

#include <future>
#include <nlohmann/json.hpp>

#include "LocationData.h"

class N1N2MessageSubscription {
 public:
  std::string supi, id;

  static std::shared_ptr<N1N2MessageSubscription> create(
      const std::string& supi) {
    return std::make_shared<N1N2MessageSubscription>(supi);
  }

  N1N2MessageSubscription(const std::string& supi) : supi{supi} {
    this->subscribe();
  }

  ~N1N2MessageSubscription() {
    if (this->is_subscribed()) {
      this->unsubscribe();
    }
  }

  bool is_subscribed() const {
    return !this->supi.empty() && !this->id.empty();
  }

  bool subscribe(std::string supi = "");
  bool unsubscribe();
};

class LMFContext {
 public:
  LMFContext(std::string supi) : supi{supi} {}

  void finish();
  std::promise<nlohmann::json> promise;

 private:
  std::string supi;
};

#endif
