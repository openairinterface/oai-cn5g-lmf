<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# RELEASE NOTES:

## v2.2.1 -- April 2026

* Build and CI fixes for RHEL 9 based environments
  - Switch container registry usage to `registry.redhat.io`
  - Replace RHEL `yq` image pull with direct GitHub download flow
  - Pass the EPEL URL as a parameter in Jenkins RHEL jobs
* Licensing and documentation
  - Re-license the project from OAI Public License v1.1 to CSSL v1.0
  - Re-license documentation under CC-BY-4.0 and orchestration/CI assets under MIT
  - Add `NOTICE`, `LICENSES/`, and related contribution/documentation updates
* Features
  - Add a basic linear least-squares estimator for TDoA (Time Difference of Arrival) positioning, including improved matrix inversion using LAPACK/BLAS

## v2.2.0 -- December 2025

* Fixes
  - Fix gNB ID encoding
  - Update LTTNG to v0.15.0 to avoid build issues
* Future Fixes
  - Add support for Ubuntu 24.04
  - Add support for RHEL 10, update container images to UBI 10
  - Fix build issue in non-containerized environment

## v2.1.0 -- August 2024

* Initial Relase
