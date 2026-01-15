#ifndef POSITION_ESTIMATION_HPP
#define POSITION_ESTIMATION_HPP

// Include necessary libraries
#include <cblas.h> // For cblas_dgemm
#include <lapacke.h> // For LAPACKE_dgetrf and LAPACKE_dgetri

#ifdef __cplusplus
extern "C" {
#endif

int inverse_matrix(double A[2][2], double A_inv[2][2]);

void lls_estimation(double trp_pos[][3], int trp_pos_size, double dd_estimated[], int dd_estimated_size, double pos_est[2]);

#ifdef __cplusplus
}
#endif

#endif // POSITION_ESTIMATION_HPP
