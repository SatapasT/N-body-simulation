#include "IO.h"
#include "NBodySimulation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <omp.h>

namespace fs = std::filesystem;

static void directoryExists(const std::string &dirPath) {
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

static inline void mergeBodiesInPlace(std::vector<Body>& bodies, int i, int j) {
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

static inline int64_t floor_div_to_int(double x, double invCell) {
  return static_cast<int64_t>(std::floor(x * invCell));
}

static inline uint64_t hash3i(int64_t a, int64_t b, int64_t c) {
  auto mix = [](uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
  };

  uint64_t ua = mix(static_cast<uint64_t>(a) + 0x9e3779b97f4a7c15ULL);
  uint64_t ub = mix(static_cast<uint64_t>(b) + 0xbf58476d1ce4e5b9ULL);
  uint64_t uc = mix(static_cast<uint64_t>(c) + 0x94d049bb133111ebULL);

  return ua ^ (ub << 1) ^ (uc << 2);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " input_file.txt\n";
    return 1;
  }

  NBodySimulation nbs(argv[1]);

  directoryExists("paraview-output");

  openPVDFile();
  int snapshotCounter = 0;
  writeVTKSnapshot(nbs, snapshotCounter);
  addSnapshotToPVD(snapshotCounter);

  double t = 0.0;
  double nextPlotTime = nbs.tPlotDelta;

  const int N0 = static_cast<int>(nbs.bodies.size());
  long long totalMerges = 0;

  std::vector<double> forceX(N0, 0.0);
  std::vector<double> forceY(N0, 0.0);
  std::vector<double> forceZ(N0, 0.0);

  const double G    = 1.0;
  const double eps2 = 1e-12;

  const double C = 1e-2 / static_cast<double>(N0);

  std::unordered_map<uint64_t, std::vector<int>> grid;
  grid.reserve(static_cast<size_t>(N0) * 2);

  while (t < nbs.tFinal) {

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N0; ++i) {
      if (!isAlive(nbs.bodies[i])) {
        forceX[i] = forceY[i] = forceZ[i] = 0.0;
        continue;
      }

      const double xi = nbs.bodies[i].x[0];
      const double yi = nbs.bodies[i].x[1];
      const double zi = nbs.bodies[i].x[2];
      const double mi = nbs.bodies[i].mass;

      double fx = 0.0, fy = 0.0, fz = 0.0;

      #pragma omp simd reduction(+:fx,fy,fz)
      for (int j = 0; j < N0; ++j) {
        if (j == i) continue;
        const double mj = nbs.bodies[j].mass;
        if (mj <= 0.0) continue;

        const double dx = nbs.bodies[j].x[0] - xi;
        const double dy = nbs.bodies[j].x[1] - yi;
        const double dz = nbs.bodies[j].x[2] - zi;

        const double r2    = dx*dx + dy*dy + dz*dz + eps2;
        const double invR  = 1.0 / std::sqrt(r2);
        const double invR3 = invR * invR * invR;

        const double s = (G * mi * mj * invR3);

        fx += s * dx;
        fy += s * dy;
        fz += s * dz;
      }

      forceX[i] = fx;
      forceY[i] = fy;
      forceZ[i] = fz;
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

    bool merged = true;
    while (merged) {
      merged = false;

      double mmax = 0.0;
      for (int i = 0; i < N0; ++i) {
        if (nbs.bodies[i].mass > mmax) mmax = nbs.bodies[i].mass;
      }
      double maxR = C * (mmax + mmax);

      if (maxR <= 0.0) break;

      grid.clear();
      const double invCell = 1.0 / maxR;

      for (int i = 0; i < N0; ++i) {
        if (!isAlive(nbs.bodies[i])) continue;

        const int64_t cx = floor_div_to_int(nbs.bodies[i].x[0], invCell);
        const int64_t cy = floor_div_to_int(nbs.bodies[i].x[1], invCell);
        const int64_t cz = floor_div_to_int(nbs.bodies[i].x[2], invCell);

        const uint64_t key = hash3i(cx, cy, cz);
        grid[key].push_back(i);
      }

      for (int i = 0; i < N0 && !merged; ++i) {
        if (!isAlive(nbs.bodies[i])) continue;

        const double xi = nbs.bodies[i].x[0];
        const double yi = nbs.bodies[i].x[1];
        const double zi = nbs.bodies[i].x[2];
        const double mi = nbs.bodies[i].mass;

        const int64_t cx = floor_div_to_int(xi, invCell);
        const int64_t cy = floor_div_to_int(yi, invCell);
        const int64_t cz = floor_div_to_int(zi, invCell);

        // Check own cell + 26 neighbours
        for (int dz = -1; dz <= 1 && !merged; ++dz) {
          for (int dy = -1; dy <= 1 && !merged; ++dy) {
            for (int dx = -1; dx <= 1 && !merged; ++dx) {
              const uint64_t key = hash3i(cx + dx, cy + dy, cz + dz);
              auto it = grid.find(key);
              if (it == grid.end()) continue;

              const auto& candidates = it->second;
              for (int idx = 0; idx < (int)candidates.size() && !merged; ++idx) {
                const int j = candidates[idx];
                if (j <= i) continue;                 // preserve (i<j) ordering
                if (!isAlive(nbs.bodies[j])) continue;

                const double mj = nbs.bodies[j].mass;

                const double dxp = nbs.bodies[j].x[0] - xi;
                const double dyp = nbs.bodies[j].x[1] - yi;
                const double dzp = nbs.bodies[j].x[2] - zi;

                const double dist2   = dxp*dxp + dyp*dyp + dzp*dzp;
                const double thresh  = C * (mi + mj);
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
      }
    }

    t += nbs.dt;

    if (t >= nextPlotTime) {
      snapshotCounter++;
      writeVTKSnapshot(nbs, snapshotCounter);
      addSnapshotToPVD(snapshotCounter);
      nextPlotTime += nbs.tPlotDelta;
    }
  }

  closePVDFile();
  std::cout << "Simulation completed.\n";
  return 0;
}