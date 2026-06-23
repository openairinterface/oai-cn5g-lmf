/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * 3D linear least-squares TDOA position estimation (x, y, z, r0).
 *
 * Replaces the original position_estimation.cpp. NOTE: the signature changed —
 * pos_est is now double[3] (was double[2]). Update position_estimation.hpp and
 * the one call site in lmf_location_determination.cpp accordingly (see notes).
 *
 * Why 3D: the EURECOM TRPs sit at two heights (z = 1.7 m and 12.5 m) and the
 * measured UL-RTOA delays are true 3D path lengths. Solving in 2D (the original
 * behaviour) is internally inconsistent and leaves a ~1.7 m residual even with
 * perfect timing; solving in 3D removes it.
 *
 * Correct linear TDOA equation, reference TRP index 0, d_i = r_i - r_0:
 *   2(xi-x0)x + 2(yi-y0)y + 2(zi-z0)z + 2 d_i r0
 *        = (xi^2-x0^2)+(yi^2-y0^2)+(zi^2-z0^2) - d_i^2
 * Unknowns: x, y, z, r0  ->  need >= 5 TRPs. EURECOM has 8 (7 equations). OK.
 *
 * Units: trp_pos and dd_estimated must share a length unit. In the LMF path
 * both are centimetres (Units=cm, c = 30.0 cm/ns), so output is in cm.
 */

#include "position_estimation.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <lapacke.h>

int inverse_matrix(double A[2][2], double A_inv[2][2]) {
  constexpr int n = 2, lda = n;
  std::memcpy(A_inv, A, sizeof(double) * n * n);
  int ipiv[2];
  int info = LAPACKE_dgetrf(LAPACK_ROW_MAJOR, n, n, &A_inv[0][0], lda, ipiv);
  if (info != 0) { std::printf("dgetrf info=%d\n", info); return -1; }
  info = LAPACKE_dgetri(LAPACK_ROW_MAJOR, n, &A_inv[0][0], lda, ipiv);
  if (info != 0) { std::printf("dgetri info=%d\n", info); return -1; }
  return 0;
}

// 3D TDOA least squares. dd_estimated[i] = r_{i+1} - r_0 (reference = TRP 0).
void lls_estimation(
    double trp_pos[][3], int trp_pos_size, double dd_estimated[],
    int dd_estimated_size, double pos_est[3]) {
  pos_est[0] = pos_est[1] = pos_est[2] = 0.0;

  const int m = trp_pos_size - 1;  // equations
  const int n = 4;                 // x, y, z, r0

  if (m < n) {
    std::printf("LLS(3D): need >= %d TRPs, got %d.\n", n + 1, trp_pos_size);
    return;
  }
  if (dd_estimated_size < m) {
    std::printf("LLS(3D): dd_estimated_size %d < %d.\n", dd_estimated_size, m);
    return;
  }

  const double x0 = trp_pos[0][0], y0 = trp_pos[0][1], z0 = trp_pos[0][2];

  std::vector<double> A(static_cast<size_t>(m) * n, 0.0);
  std::vector<double> b(static_cast<size_t>(m > n ? m : n), 0.0);  // dgels needs max(m,n)

  for (int i = 1; i < trp_pos_size; ++i) {
    const double xi = trp_pos[i][0], yi = trp_pos[i][1], zi = trp_pos[i][2];
    const double di = dd_estimated[i - 1];
    const int    r  = i - 1;
    A[r * n + 0] = 2.0 * (xi - x0);
    A[r * n + 1] = 2.0 * (yi - y0);
    A[r * n + 2] = 2.0 * (zi - z0);
    A[r * n + 3] = 2.0 * di;
    b[r] = (xi*xi - x0*x0) + (yi*yi - y0*y0) + (zi*zi - z0*z0) - di*di;
  }

  int info = LAPACKE_dgels(
      LAPACK_ROW_MAJOR, 'N', m, n, 1, A.data(), n, b.data(), 1);
  if (info != 0) {
    std::printf("LLS(3D) dgels failed: info=%d (degenerate geometry?)\n", info);
    return;
  }

  pos_est[0] = b[0];  // x
  pos_est[1] = b[1];  // y
  pos_est[2] = b[2];  // z
  std::printf("[pos_est] x=%.3f y=%.3f z=%.3f r0=%.3f (TRP position units)\n",
              b[0], b[1], b[2], b[3]);
}
