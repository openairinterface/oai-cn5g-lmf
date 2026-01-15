#include "position_estimation.hpp"
#include <cmath>
#include <cstdio>
#include <cblas.h>
#include <lapacke.h>

// Function to invert a matrix A using LAPACKE_dgetrf and LAPACKE_dgetri
int inverse_matrix(double A[2][2], double A_inv[2][2]) {
    int n = 2; // Number of rows/columns
    int lda = n; // Leading dimension of the array

    int ipiv[2]; // Pivot indices for LU factorization
    int info;

    // Compute LU factorization of A
    info = LAPACKE_dgetrf(LAPACK_ROW_MAJOR, n, n, &A[0][0], lda, ipiv);
    if (info != 0) {
        printf("Matrix inversion failed: LU factorization returned non-zero info value.\n");
        return -1;
    }

    // Compute inverse of A using LU factorization
    info = LAPACKE_dgetri(LAPACK_ROW_MAJOR, n, &A[0][0], lda, ipiv);
    if (info != 0) {
        printf("Matrix inversion failed: LAPACKE_dgetri returned non-zero info value.\n");
        return -1;
    }

    // Copy the inverted matrix to A_inv
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            A_inv[i][j] = A[i][j];
        }
    }

    return 0;
}

// Function to perform LLS estimation
void lls_estimation(double trp_pos[][3], int trp_pos_size, double dd_estimated[], int dd_estimated_size, double pos_est[2]) {
    double A[trp_pos_size-1][2];
    double b[trp_pos_size-1];
    
    for (int i = 1; i < trp_pos_size; i++) {
        A[i-1][0] = 2*(trp_pos[i][0] - trp_pos[0][0]);
        A[i-1][1] = 2*(trp_pos[i][1] - trp_pos[0][1]);
        b[i-1] = pow(trp_pos[i][0], 2) - pow(trp_pos[0][0], 2) + pow(trp_pos[i][1], 2) - pow(trp_pos[0][1], 2) - pow(dd_estimated[i-1], 2);
    }

    double AtA[2][2] = {{0.0, 0.0}, {0.0, 0.0}};
    double Atb[2] = {0.0, 0.0};

    for (int i = 0; i < trp_pos_size-1; i++) {
        AtA[0][0] += A[i][0] * A[i][0];
        AtA[0][1] += A[i][0] * A[i][1];
        AtA[1][0] += A[i][1] * A[i][0];
        AtA[1][1] += A[i][1] * A[i][1];
        Atb[0] += A[i][0] * b[i];
        Atb[1] += A[i][1] * b[i];
    }

    double AtA_inv[2][2];
    if (inverse_matrix(AtA, AtA_inv) != 0) {
        printf("LLS matrix inversion failed.\n");
        return;
    }

    pos_est[0] = AtA_inv[0][0] * Atb[0] + AtA_inv[0][1] * Atb[1];
    pos_est[1] = AtA_inv[1][0] * Atb[0] + AtA_inv[1][1] * Atb[1];

    pos_est[0] /= 2.0;
    pos_est[1] /= 2.0;

    pos_est[0] += trp_pos[0][0];
    pos_est[1] += trp_pos[0][1];
}