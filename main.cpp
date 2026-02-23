#include "IO.h"
#include "NBodySimulation.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>
#include <omp.h>
#include <algorithm>

namespace fs = std::filesystem;

void directoryExists(const std::string &dirPath) {
  if (!fs::exists(dirPath)) {
    if (fs::create_directory(dirPath)) {
      std::cout << "Created directory: " << dirPath << std::endl;
    } else {
      std::cerr << "Failed to create directory: " << dirPath << std::endl;
    }
  }
}

static inline bool isAlive(const Body& b) {
  return b.mass > 0.0;
}

static void mergeBodiesInPlace(std::vector<Body>& bodies, int i, int j) {
  const double mi = bodies[i].mass;
  const double mj = bodies[j].mass;
  const double m  = mi + mj;

  for (int d = 0; d < 3; ++d) {
    bodies[i].x[d] = (mi * bodies[i].x[d] + mj * bodies[j].x[d]) / m;
    bodies[i].v[d] = (mi * bodies[i].v[d] + mj * bodies[j].v[d]) / m;
  }
  bodies[i].mass = m;

  bodies[j].mass = 0.0;
  bodies[j].v[0] = bodies[j].v[1] = bodies[j].v[2] = 0.0;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " input_file.txt\n";
    return 1;
  }
  NBodySimulation nbs(argv[1]);

  directoryExists("paraview-output");

  // Write initial snapshot
  openPVDFile();
  int snapshotCounter = 0;
  writeVTKSnapshot(nbs, snapshotCounter);
  addSnapshotToPVD(snapshotCounter);

  // Initialise simulation at t = 0
  double t = 0.0;
  double nextPlotTime = nbs.tPlotDelta;

  const int N0 = static_cast<int>(nbs.bodies.size());

  long long totalMerges = 0;

  auto countAlive = [&](const std::vector<Body>& bodies) {
    int alive = 0;
    for (const auto& b : bodies) if (b.mass > 0.0) ++alive;
    return alive;
  };

  constexpr int BS = 256;               
  const int NB = (N0 + BS - 1) / BS;     

  std::vector<omp_lock_t> blockLocks(NB);
  for (int b = 0; b < NB; ++b) omp_init_lock(&blockLocks[b]);

  std::vector<double> forceX(N0, 0.0);
  std::vector<double> forceY(N0, 0.0);
  std::vector<double> forceZ(N0, 0.0);

  while (t < nbs.tFinal) {
    const double G    = 1.0;
    const double eps2 = 1e-12;

    std::fill(forceX.begin(), forceX.end(), 0.0);
    std::fill(forceY.begin(), forceY.end(), 0.0);
    std::fill(forceZ.begin(), forceZ.end(), 0.0);

    // ============================================================
    // Forces + Veclocity/Position
    // ============================================================
    #pragma omp parallel
    {
      std::vector<double> fx_i(BS), fy_i(BS), fz_i(BS);
      std::vector<double> fx_j(BS), fy_j(BS), fz_j(BS);

      #pragma omp for collapse(2) schedule(static)
      for (int bi = 0; bi < NB; ++bi) {
        for (int bj = 0; bj < NB; ++bj) {
          if (bj < bi) continue;

          const int i0 = bi * BS;
          const int i1 = std::min(i0 + BS, N0);
          const int j0 = bj * BS;
          const int j1 = std::min(j0 + BS, N0);

          const int ni = i1 - i0;
          const int nj = j1 - j0;

          std::fill(fx_i.begin(), fx_i.begin() + ni, 0.0);
          std::fill(fy_i.begin(), fy_i.begin() + ni, 0.0);
          std::fill(fz_i.begin(), fz_i.begin() + ni, 0.0);

          std::fill(fx_j.begin(), fx_j.begin() + nj, 0.0);
          std::fill(fy_j.begin(), fy_j.begin() + nj, 0.0);
          std::fill(fz_j.begin(), fz_j.begin() + nj, 0.0);

          if (bi == bj) {
            for (int ii = 0; ii < ni; ++ii) {
              const int i = i0 + ii;
              if (!isAlive(nbs.bodies[i])) continue;

              const double xi = nbs.bodies[i].x[0];
              const double yi = nbs.bodies[i].x[1];
              const double zi = nbs.bodies[i].x[2];
              const double mi = nbs.bodies[i].mass;

              #pragma omp simd
              for (int jj = ii + 1; jj < nj; ++jj) {
                const int j = j0 + jj;

                const double mj = nbs.bodies[j].mass;
                const double alive = (mj > 0.0) ? 1.0 : 0.0;

                const double dx = nbs.bodies[j].x[0] - xi;
                const double dy = nbs.bodies[j].x[1] - yi;
                const double dz = nbs.bodies[j].x[2] - zi;

                const double r2    = dx*dx + dy*dy + dz*dz + eps2;
                const double invR  = 1.0 / std::sqrt(r2);
                const double invR3 = invR * invR * invR;

                const double s  = alive * (G * mi * mj * invR3);

                const double fx = s * dx;
                const double fy = s * dy;
                const double fz = s * dz;

                fx_i[ii] += fx;  fy_i[ii] += fy;  fz_i[ii] += fz;
                fx_j[jj] -= fx;  fy_j[jj] -= fy;  fz_j[jj] -= fz;
              }
            }

            omp_set_lock(&blockLocks[bi]);
            for (int k = 0; k < ni; ++k) {
              const int idx = i0 + k;
              forceX[idx] += fx_i[k] + fx_j[k];
              forceY[idx] += fy_i[k] + fy_j[k];
              forceZ[idx] += fz_i[k] + fz_j[k];
            }
            omp_unset_lock(&blockLocks[bi]);

          } else {
            for (int ii = 0; ii < ni; ++ii) {
              const int i = i0 + ii;
              if (!isAlive(nbs.bodies[i])) continue;

              const double xi = nbs.bodies[i].x[0];
              const double yi = nbs.bodies[i].x[1];
              const double zi = nbs.bodies[i].x[2];
              const double mi = nbs.bodies[i].mass;

              #pragma omp simd
              for (int jj = 0; jj < nj; ++jj) {
                const int j = j0 + jj;

                const double mj = nbs.bodies[j].mass;
                const double alive = (mj > 0.0) ? 1.0 : 0.0;

                const double dx = nbs.bodies[j].x[0] - xi;
                const double dy = nbs.bodies[j].x[1] - yi;
                const double dz = nbs.bodies[j].x[2] - zi;

                const double r2    = dx*dx + dy*dy + dz*dz + eps2;
                const double invR  = 1.0 / std::sqrt(r2);
                const double invR3 = invR * invR * invR;

                const double s  = alive * (G * mi * mj * invR3);

                const double fx = s * dx;
                const double fy = s * dy;
                const double fz = s * dz;

                fx_i[ii] += fx;  fy_i[ii] += fy;  fz_i[ii] += fz;
                fx_j[jj] -= fx;  fy_j[jj] -= fy;  fz_j[jj] -= fz;
              }
            }

            if (bi < bj) {
              omp_set_lock(&blockLocks[bi]);
              omp_set_lock(&blockLocks[bj]);

              for (int k = 0; k < ni; ++k) {
                const int idx = i0 + k;
                forceX[idx] += fx_i[k];
                forceY[idx] += fy_i[k];
                forceZ[idx] += fz_i[k];
              }
              for (int k = 0; k < nj; ++k) {
                const int idx = j0 + k;
                forceX[idx] += fx_j[k];
                forceY[idx] += fy_j[k];
                forceZ[idx] += fz_j[k];
              }

              omp_unset_lock(&blockLocks[bj]);
              omp_unset_lock(&blockLocks[bi]);
            } else {
              omp_set_lock(&blockLocks[bj]);
              omp_set_lock(&blockLocks[bi]);

              for (int k = 0; k < ni; ++k) {
                const int idx = i0 + k;
                forceX[idx] += fx_i[k];
                forceY[idx] += fy_i[k];
                forceZ[idx] += fz_i[k];
              }
              for (int k = 0; k < nj; ++k) {
                const int idx = j0 + k;
                forceX[idx] += fx_j[k];
                forceY[idx] += fy_j[k];
                forceZ[idx] += fz_j[k];
              }

              omp_unset_lock(&blockLocks[bi]);
              omp_unset_lock(&blockLocks[bj]);
            }
          }
        }
      }
    }

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N0; ++i) {
      if (!isAlive(nbs.bodies[i])) continue;

      const double invMass = 1.0 / nbs.bodies[i].mass;

      const double v0x = nbs.bodies[i].v[0];
      const double v0y = nbs.bodies[i].v[1];
      const double v0z = nbs.bodies[i].v[2];

      nbs.bodies[i].v[0] = v0x + nbs.dt * forceX[i] * invMass;
      nbs.bodies[i].v[1] = v0y + nbs.dt * forceY[i] * invMass;
      nbs.bodies[i].v[2] = v0z + nbs.dt * forceZ[i] * invMass;

      nbs.bodies[i].x[0] += nbs.dt * v0x;
      nbs.bodies[i].x[1] += nbs.dt * v0y;
      nbs.bodies[i].x[2] += nbs.dt * v0z;
    }

    // ============================================================
    // COLLISIONS
    // ============================================================
    {
      const double C = 1e-2 / static_cast<double>(N0);

      bool merged = true;
      while (merged) {
        merged = false;

        for (int i = 0; i < N0 && !merged; ++i) {
          if (!isAlive(nbs.bodies[i])) continue;

          for (int j = i + 1; j < N0 && !merged; ++j) {
            if (!isAlive(nbs.bodies[j])) continue;

            const double dx = nbs.bodies[j].x[0] - nbs.bodies[i].x[0];
            const double dy = nbs.bodies[j].x[1] - nbs.bodies[i].x[1];
            const double dz = nbs.bodies[j].x[2] - nbs.bodies[i].x[2];

            const double dist2   = dx*dx + dy*dy + dz*dz;
            const double thresh  = C * (nbs.bodies[i].mass + nbs.bodies[j].mass);
            const double thresh2 = thresh * thresh;

            if (dist2 <= thresh2) {
              mergeBodiesInPlace(nbs.bodies, i, j);
              ++totalMerges;
              merged = true;
            }
          }
        }
      }
    }

    t += nbs.dt;

    if (t >= nextPlotTime) {
      snapshotCounter++;
      writeVTKSnapshot(nbs, snapshotCounter);
      addSnapshotToPVD(snapshotCounter);
      nextPlotTime += nbs.tPlotDelta;

      const int aliveNow = countAlive(nbs.bodies);
      std::cout << "Plot next snapshot"
                << ",\t t=" << t
                << ",\t dt=" << nbs.dt
                << ",\t alive=" << aliveNow
                << ",\t merges=" << totalMerges
                << std::endl;
    }
  }

  for (int b = 0; b < NB; ++b) omp_destroy_lock(&blockLocks[b]);
  closePVDFile();
  std::cout << "Simulation completed.\n";

  return 0;
}
