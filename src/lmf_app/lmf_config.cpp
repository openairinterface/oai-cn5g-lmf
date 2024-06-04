/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this
 *file except in compliance with the License. You may obtain a copy of the
 *License at
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

#include "lmf_config.hpp"

#include "string.hpp"
#include <iostream>
#include <libconfig.h++>

#include "fqdn.hpp"
#include "if.hpp"
#include "logger.hpp"

#include "string.hpp"

#include <arpa/inet.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include "common_defs.h"

using namespace libconfig;

namespace oai::lmf::config {

//------------------------------------------------------------------------------
lmf_config::lmf_config() : sbi() {
  use_fqdn_dns = false;
  use_http2    = false;
}

//------------------------------------------------------------------------------
lmf_config::~lmf_config() {}

}  // namespace oai::lmf::config
