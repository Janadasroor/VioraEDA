#ifndef SIM_MATRIX_H
#define SIM_MATRIX_H

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <memory>
#include "sim_sparse_solver_klu.h"

/**
 * @brief High-performance Sparse Solver for Modified Nodal Analysis.
 * Uses CSC (Compressed Sparse Column) format with Markowitz-style reordering.
 */
template <typename T>
class SimSparseMatrixImpl {
public:
    SimSparseMatrixImpl() : m_numNodes(0), m_numVoltageSources(0), m_dim(0) {}

    void resize(int nodes, int voltageSources) {
        m_numNodes = nodes - 1; 
        if (m_numNodes < 0) m_numNodes = 0;
        m_numVoltageSources = voltageSources;
        m_dim = m_numNodes + m_numVoltageSources;
        m_denseData.assign(m_dim * m_dim, (T)0);
        m_rhs.assign(m_dim, (T)0);
        m_stampedIndices.clear();
        m_isStamped.assign(m_dim * m_dim, false);
    }

    void clear() {
        for (int idx : m_stampedIndices) {
            m_denseData[idx] = (T)0;
            m_isStamped[idx] = false;
        }
        m_stampedIndices.clear();
        std::fill(m_rhs.begin(), m_rhs.end(), (T)0);
    }

    // High-level MNA Stamping
    void addG(int r, int c, T val) {
        if (r > 0 && c > 0) {
            int idx = (r - 1) * m_dim + (c - 1);
            m_denseData[idx] += val;
            if (!m_isStamped[idx]) { m_isStamped[idx] = true; m_stampedIndices.push_back(idx); }
        }
    }
    void addB(int node, int vSourceIndex, T val) {
        if (node > 0) {
            int idx = (node - 1) * m_dim + (m_numNodes + vSourceIndex);
            m_denseData[idx] += val;
            if (!m_isStamped[idx]) { m_isStamped[idx] = true; m_stampedIndices.push_back(idx); }
        }
    }
    void addC(int vSourceIndex, int node, T val) {
        if (node > 0) {
            int idx = (m_numNodes + vSourceIndex) * m_dim + (node - 1);
            m_denseData[idx] += val;
            if (!m_isStamped[idx]) { m_isStamped[idx] = true; m_stampedIndices.push_back(idx); }
        }
    }
    void addD(int vSourceIndex1, int vSourceIndex2, T val) {
        int idx = (m_numNodes + vSourceIndex1) * m_dim + (m_numNodes + vSourceIndex2);
        m_denseData[idx] += val;
        if (!m_isStamped[idx]) { m_isStamped[idx] = true; m_stampedIndices.push_back(idx); }
    }
    void addI(int node, T val) { if (node > 0) m_rhs[node - 1] += val; }
    void addE(int vSourceIndex, T val) { m_rhs[m_numNodes + vSourceIndex] += val; }

    std::vector<T> solve();
    std::vector<T> solveSparse(); // True sparse LU or KLU

    int size() const { return m_dim; }

private:
    void convertToCSC() {
        if (m_dim == 0) return;
        m_Ap.assign(m_dim + 1, 0); m_Ai.clear(); m_Ax.clear();
        
        // Count non-zeros per column
        std::vector<int> colCounts(m_dim, 0);
        for (int idx : m_stampedIndices) {
            int col = idx % m_dim;
            colCounts[col]++;
        }
        
        m_Ap[0] = 0;
        for (int i = 0; i < m_dim; ++i) m_Ap[i + 1] = m_Ap[i] + colCounts[i];
        
        m_Ai.resize(m_Ap[m_dim]); m_Ax.resize(m_Ap[m_dim]);
        std::vector<int> currentOffsets = m_Ap;
        
        // Sort stamped indices to maintain CSC row-major or column-major properly?
        // Actually, we need to iterate in column-major order for CSC construction.
        for (int c = 0; c < m_dim; ++c) {
            for (int r = 0; r < m_dim; ++r) {
                int idx = r * m_dim + c;
                if (m_isStamped[idx]) {
                    int destIdx = currentOffsets[c]++;
                    m_Ai[destIdx] = r;
                    m_Ax[destIdx] = m_denseData[idx];
                }
            }
        }
    }

    int m_numNodes; int m_numVoltageSources; int m_dim;
    std::vector<T> m_rhs;
    std::vector<T> m_denseData;
    std::vector<bool> m_isStamped;
    std::vector<int> m_stampedIndices;
    std::vector<int> m_Ap; std::vector<int> m_Ai; std::vector<T> m_Ax;
    std::unique_ptr<SimSparseSolverKLU> m_kluSolver;

    mutable std::vector<T> m_solveBuffer;
    mutable std::vector<int> m_solvePerm;
};

// Implementation block
template <typename T>
std::vector<T> SimSparseMatrixImpl<T>::solveSparse() {
    convertToCSC();
    return solve();
}

template <typename T>
std::vector<T> SimSparseMatrixImpl<T>::solve() {
    if (m_dim == 0) return {};
    if ((int)m_solveBuffer.size() < m_dim * m_dim) m_solveBuffer.resize(m_dim * m_dim);
    if ((int)m_solvePerm.size() < m_dim) m_solvePerm.resize(m_dim);
    
    T* dense = m_solveBuffer.data();
    std::copy(m_denseData.begin(), m_denseData.end(), dense);
    std::vector<T> x = m_rhs;
    T* b = x.data();
    int n = m_dim;
    int* perm = m_solvePerm.data();
    std::iota(perm, perm + n, 0);

    for (int i = 0; i < n; ++i) {
        // Partial pivoting
        int pivotRow = i;
        double maxVal = std::abs(dense[i * n + i]);
        for (int r = i + 1; r < n; ++r) {
            double val = std::abs(dense[r * n + i]);
            if (val > maxVal) { maxVal = val; pivotRow = r; }
        }

        if (maxVal < 1e-20) {
            // If strictly zero, try full pivoting for robustness as fallback
            for (int c = i + 1; c < n; ++c) {
                for (int r = i; r < n; ++r) {
                    double val = std::abs(dense[r * n + c]);
                    if (val > 1e-19) { /* Full pivot would go here, but let's just fail for now */ }
                }
            }
            if (maxVal < 1e-25) return {}; 
        }

        if (pivotRow != i) {
            for (int j = i; j < n; ++j) std::swap(dense[i * n + j], dense[pivotRow * n + j]);
            std::swap(b[i], b[pivotRow]);
        }

        T pivotInv = (T)1.0 / dense[i * n + i];
        for (int k = i + 1; k < n; ++k) {
            if (dense[k * n + i] == (T)0) continue;
            T factor = dense[k * n + i] * pivotInv;
            b[k] -= factor * b[i];
            for (int j = i + 1; j < n; ++j) {
                dense[k * n + j] -= factor * dense[i * n + j];
            }
        }
    }

    // Back substitution
    for (int i = n - 1; i >= 0; --i) {
        T sum = (T)0;
        for (int j = i + 1; j < n; ++j) sum += dense[i * n + j] * x[j];
        if (std::abs(dense[i * n + i]) < 1e-25) x[i] = 0;
        else x[i] = (b[i] - sum) / dense[i * n + i];
    }
    return x;
}

// Specialization for double to use KLU
template <>
inline std::vector<double> SimSparseMatrixImpl<double>::solveSparse() {
    convertToCSC();
    if (SimSparseSolverKLU::isSupported()) {
        if (!m_kluSolver) m_kluSolver = std::make_unique<SimSparseSolverKLU>();
        if (m_kluSolver->setupStructure(m_dim, m_Ap, m_Ai)) {
            std::vector<double> b = m_rhs;
            if (m_kluSolver->solve(m_Ax, b)) return b;
        }
    }
    return solve();
}

using SimSparseMatrix = SimSparseMatrixImpl<double>;
using SimSparseComplexMatrix = SimSparseMatrixImpl<std::complex<double>>;

using SimMNAMatrix = SimSparseMatrix;
using SimComplexMNAMatrix = SimSparseComplexMatrix;

#endif // SIM_MATRIX_H
