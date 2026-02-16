#include "IO.h"
#include "NBodySimulation.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

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

static void mergeBodies(std::vector<Body> &bodies, int i, int j) {
  const double mi = bodies[i].mass;
  const double mj = bodies[j].mass;
  const double m = mi + mj;

  bodies[i].x[0] = (mi * bodies[i].x[0] + mj * bodies[j].x[0]) / m;
  bodies[i].x[1] = (mi * bodies[i].x[1] + mj * bodies[j].x[1]) / m;
  bodies[i].x[2] = (mi * bodies[i].x[2] + mj * bodies[j].x[2]) / m;

  bodies[i].v[0] = (mi * bodies[i].v[0] + mj * bodies[j].v[0]) / m;
  bodies[i].v[1] = (mi * bodies[i].v[1] + mj * bodies[j].v[1]) / m;
  bodies[i].v[2] = (mi * bodies[i].v[2] + mj * bodies[j].v[2]) / m;

  bodies[i].mass = m;

  bodies.erase(bodies.begin() + j);
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

  const int N = static_cast<int>(nbs.bodies.size());

  while (t < nbs.tFinal) {
    const int nCurrent = static_cast<int>(nbs.bodies.size());
    std::vector<double> forceX(nCurrent, 0.0);
    std::vector<double> forceY(nCurrent, 0.0);
    std::vector<double> forceZ(nCurrent, 0.0);

    const double G = 1;
    const double eps2 = 1e-12;

    // ===============================
    // Compute Forces + Sum Forces
    // ===============================
    for (int i = 0; i < nCurrent; ++i) {
      for (int j = i + 1; j < nCurrent; ++j) {
        const double dx = nbs.bodies[j].x[0] - nbs.bodies[i].x[0];
        const double dy = nbs.bodies[j].x[1] - nbs.bodies[i].x[1];
        const double dz = nbs.bodies[j].x[2] - nbs.bodies[i].x[2];

        const double r2 = (dx * dx) + (dy * dy) + (dz * dz) + eps2;
        const double invR = 1.0 / std::sqrt(r2);
        const double invR3 = invR * invR * invR;

        const double mi = nbs.bodies[i].mass;
        const double mj = nbs.bodies[j].mass;
        const double factor = G * mi * mj * invR3;

        const double fx = factor * dx;
        const double fy = factor * dy;
        const double fz = factor * dz;

        forceX[i] += fx;
        forceY[i] += fy;
        forceZ[i] += fz;
        forceX[j] -= fx;
        forceY[j] -= fy;
        forceZ[j] -= fz;
      }
    }

    // ===============================
    // Update velocities and Positions
    // ===============================
    for (int i = 0; i < nCurrent; ++i) {
      const double invMass = 1.0 / nbs.bodies[i].mass;

      // save old velocity for position update
      const double v0x = nbs.bodies[i].v[0];
      const double v0y = nbs.bodies[i].v[1];
      const double v0z = nbs.bodies[i].v[2];

      // v^{n+1} = v^n + (F^n/m)*dt
      nbs.bodies[i].v[0] = v0x + nbs.dt * forceX[i] * invMass;
      nbs.bodies[i].v[1] = v0y + nbs.dt * forceY[i] * invMass;
      nbs.bodies[i].v[2] = v0z + nbs.dt * forceZ[i] * invMass;

      // x^{n+1} = x^n + v^n*dt  (explicit Euler position update)
      nbs.bodies[i].x[0] += nbs.dt * nbs.bodies[i].v[0];
      nbs.bodies[i].x[1] += nbs.dt * nbs.bodies[i].v[1];
      nbs.bodies[i].x[2] += nbs.dt * nbs.bodies[i].v[2];
    }

    // ===============================
    // Collision Handling
    // ===============================
    if (nbs.bodies.size() >= 2) {
      const double C = 1e-2 / static_cast<double>(N);

      bool mergeable = true;
      while (mergeable) {
        mergeable = false;

        const int nNow = static_cast<int>(nbs.bodies.size());
        for (int i = 0; i < nNow && !mergeable; ++i) {
          for (int j = i + 1; j < nNow && !mergeable; ++j) {
            const double dx = nbs.bodies[j].x[0] - nbs.bodies[i].x[0];
            const double dy = nbs.bodies[j].x[1] - nbs.bodies[i].x[1];
            const double dz = nbs.bodies[j].x[2] - nbs.bodies[i].x[2];

            const double distanceSqr = dx * dx + dy * dy + dz * dz;
            const double thresh = C * (nbs.bodies[i].mass + nbs.bodies[j].mass);
            const double thresh2 = thresh * thresh;

            if (distanceSqr <= thresh2) {
              mergeBodies(nbs.bodies, i, j);
              mergeable = true;
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
      std::cout << "Plot next snapshot" << ",\t t=" << t << ",\t dt=" << nbs.dt
                << ",\t N=" << nbs.bodies.size() << std::endl;
      // In addition to the above quantities you may want to track maximum
      // velocity and smallest distanceance between masses. This is particularly
      // useful when implementing collisions.
    }
  }

  closePVDFile();
  std::cout << "Simulation completed.\n";

  return 0;
}
