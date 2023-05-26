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
    if cli == 'docker':
        cmd = f'sed -i -e "s@oaisoftwarealliance/oai-lmf:develop@{tag}@" ci-scripts/docker-compose/sanity-check/docker-compose.yaml'
        myCmds.run(cmd)
        cmd = 'cd ci-scripts/docker-compose/sanity-check && docker-compose up -d oai-nrf'
        upStatus = myCmds.run(cmd)
        for line in upStatus.stdout.split('\n'):
            print(line)
        time.sleep(5)
        cmd = 'sudo rm -f /tmp/sanity-lmf-ubuntu.*'
        cmd = 'nohup sudo tshark -i sanity-oai -f "sctp or port 80 or port 8080 or port 8805 or icmp or port 3306" -w /tmp/sanity-lmf-ubuntu.pcap > /tmp/sanity-lmf-ubuntu.log 2>&1 &'
        myCmds.run(cmd)
        time.sleep(5)
        cmd = 'cd ci-scripts/docker-compose/sanity-check && docker-compose up -d'
        upStatus = myCmds.run(cmd)
        for line in upStatus.stdout.split('\n'):
            print(line)
        time.sleep(20)
        cmd = 'docker logs cicd-oai-lmf 2>&1 | grep REGISTERED'
        registerCheck = myCmds.run(cmd)
        if registerCheck.returncode != 0:
            status = -1
        cmd = 'docker inspect --format="STATUS: {{.State.Health.Status}}" cicd-oai-lmf'
        healthStatus = myCmds.run(cmd)
        for line in healthStatus.stdout.split('\n'):
            print(line)
            if re.search('STATUS:', line):
                if re.search('STATUS: healthy', line):
                    status = 0
                else:
                    status = -1
        cmd = 'cd ci-scripts/docker-compose/sanity-check && docker-compose stop'
        stopStatus = myCmds.run(cmd)
        for line in stopStatus.stdout.split('\n'):
            print(line)
        time.sleep(5)
        cmd = 'mkdir -p archives/sanity-check-ubuntu'
        myCmds.run(cmd)
        cmd = 'docker logs cicd-oai-nrf > archives/sanity-check-ubuntu/oai-nrf.log 2>&1'
        myCmds.run(cmd)
        cmd = 'docker logs cicd-oai-lmf > archives/sanity-check-ubuntu/oai-lmf.log 2>&1'
        myCmds.run(cmd)
        cmd = 'sudo chmod 666 /tmp/sanity-lmf-ubuntu.* && cp /tmp/sanity-lmf-ubuntu.* archives/sanity-check-ubuntu'
        myCmds.run(cmd)
        cmd = 'cd ci-scripts/docker-compose/sanity-check && docker-compose down'
        downStatus = myCmds.run(cmd)
        for line in downStatus.stdout.split('\n'):
            print(line)
        cmd = 'docker volume prune --force || true'
        myCmds.run(cmd)
    return status

if __name__ == '__main__':
    main()
