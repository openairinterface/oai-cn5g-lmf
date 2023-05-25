#!/usr/bin/env python3
"""
 Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 contributor license agreements.  See the NOTICE file distributed with
 this work for additional information regarding copyright ownership.
 The OpenAirInterface Software Alliance licenses this file to You under
 the OAI Public License, Version 1.1  (the "License"); you may not use this file
 except in compliance with the License.
 You may obtain a copy of the License at

   http://www.openairinterface.org/?page_id=698

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
-------------------------------------------------------------------------------
 For more information about the OpenAirInterface (OAI) Software Alliance:
   contact@openairinterface.org
"""

import argparse
import os
import re
import sys
import time
import common.python.cls_cmd as cls_cmd

def main() -> None:
    args = _parse_args()
    status = generic_deployment(args.tag)
    sys.exit(status)

def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description='Sanity Check')

    parser.add_argument(
        '--tag', '-t',
        action='store',
        required=True,
        help='Image Tag in image-name:image tag format',
    )
    return parser.parse_args()

def generic_deployment(tag):
    # First detect which docker/podman command to use
    cli = ''
    myCmds = cls_cmd.LocalCmd()
    cmd = 'which podman || true'
    podman_check = myCmds.run(cmd, silent=True)
    if re.search('podman', podman_check.stdout):
        cli = 'sudo podman'
    if cli == '':
        cmd = 'which docker || true'
        docker_check = myCmds.run(cmd, silent=True)
        if re.search('docker', docker_check.stdout):
            cli = 'docker'
    if cli == '':
        print ('No docker / podman installed: quitting')
        return -1

    status = 0
    return status

if __name__ == '__main__':
    main()
