/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * 3D linear least-squares TDOA position estimation (x, y, z, r0).
 *
 * Why 3D: when TRPs sit at differing heights, the measured UL-RTOA delays are
 * true 3D path lengths. Solving in 2D is internally inconsistent and leaves a
 * residual on the order of the TRP height spread even with perfect timing;
 * solving in 3D removes it.
 *
 * Linear TDOA equation, reference TRP index 0, d_i = r_i - r_0:
 *   2(xi-x0)x + 2(yi-y0)y + 2(zi-z0)z + 2 d_i r0
 *        = (xi^2-x0^2)+(yi^2-y0^2)+(zi^2-z0^2) - d_i^2
 * Unknowns: x, y, z, r0 -> requires at least 5 TRPs (4 equations).
 * With exactly 4 TRPs the UE height is fixed and (x, y, r0) are solved.
 *
 * Units: trp_pos and dd_estimated must share a length unit. The LMF caller
 * normalizes TRP coordinates to metres (any xYZunit) and uses c = 0.3 m/ns,
 * so output is in metres.
 */

#include "position_estimation.hpp"
#include <cmath>
#include <cstdio>
#include <vector>
#include <lapacke.h>

// UE height assumed when there are too few TRPs to solve for it (4 TRPs), in
// TRP position units (metres from the LMF caller). This is the height the
// former 2D solver reported.
constexpr double kFallbackUeHeight = 1.5;

// 3D TDOA least squares. dd_estimated[i] = r_{i+1} - r_0 (reference = TRP 0).
bool lls_estimation(
    double trp_pos[][3], int trp_pos_size, double dd_estimated[],
    int dd_estimated_size, double pos_est[3]) {
  pos_est[0] = pos_est[1] = pos_est[2] = 0.0;

  const int m = trp_pos_size - 1;  // equations
  const int n = 4;                 // x, y, z, r0

  // With 5 or more TRPs solve for (x, y, z, r0). With 4 TRPs there are too
  // few equations for that, so fix the UE height and solve for (x, y, r0).
  const bool solve_z = m >= n;
  if (m < n - 1) {
    std::printf("LLS: need >= %d TRPs, got %d.\n", n, trp_pos_size);
    return false;
  }
  if (dd_estimated_size < m) {
    std::printf("LLS(3D): dd_estimated_size %d < %d.\n", dd_estimated_size, m);
    return false;
  }

  const double x0 = trp_pos[0][0], y0 = trp_pos[0][1], z0 = trp_pos[0][2];

  std::vector<double> A(static_cast<size_t>(m) * n, 0.0);
  std::vector<double> b(
      static_cast<size_t>(m > n ? m : n), 0.0);  // dgels needs max(m,n)

  for (int i = 1; i < trp_pos_size; ++i) {
    const double xi = trp_pos[i][0], yi = trp_pos[i][1], zi = trp_pos[i][2];
    const double di = dd_estimated[i - 1];
    const int r     = i - 1;
    A[r * n + 0]    = 2.0 * (xi - x0);
    A[r * n + 1]    = 2.0 * (yi - y0);
    A[r * n + 2]    = 2.0 * (zi - z0);
    A[r * n + 3]    = 2.0 * di;
    b[r] = (xi * xi - x0 * x0) + (yi * yi - y0 * y0) + (zi * zi - z0 * z0) -
           di * di;
  }

  // If every range difference is zero (e.g. an ideal channel where all TRPs
  // report the same ToA), the r0 column of A is zero and r0 drops out of the
  // equations, so solve for (x, y, z) only.
  bool r0_in_system = false;
  for (int i = 0; i < m; ++i) {
    if (std::fabs(dd_estimated[i]) > 1e-9) r0_in_system = true;
  }
  int n_solve = n;
  if (!solve_z) {
    // z is known: move its term to the right-hand side and let r0 take its
    // column, so the unknowns are (x, y, r0).
    for (int r = 0; r < m; ++r) {
      b[r] -= A[r * n + 2] * kFallbackUeHeight;
      A[r * n + 2] = A[r * n + 3];
    }
    n_solve = n - 1;
  }
  if (!r0_in_system) n_solve -= 1;  // r0 is the last unknown

  int info = LAPACKE_dgels(
      LAPACK_ROW_MAJOR, 'N', m, n_solve, 1, A.data(), n, b.data(), 1);
  if (info != 0) {
    std::printf("LLS(3D) dgels failed: info=%d\n", info);
    return false;
  }

  pos_est[0] = b[0];                                // x
  pos_est[1] = b[1];                                // y
  pos_est[2] = solve_z ? b[2] : kFallbackUeHeight;  // z
  const double r0 =
      r0_in_system ? b[n_solve - 1] :
                     std::sqrt(
                         (b[0] - x0) * (b[0] - x0) + (b[1] - y0) * (b[1] - y0) +
                         (pos_est[2] - z0) * (pos_est[2] - z0));
  std::printf(
      "[pos_est] x=%.3f y=%.3f z=%.3f r0=%.3f (TRP position units)\n", b[0],
      b[1], pos_est[2], r0);
  return true;
}
