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


  while (t < nbs.tFinal) {
    std::vector<double> forceX(N0, 0.0);
    std::vector<double> forceY(N0, 0.0);
    std::vector<double> forceZ(N0, 0.0);


    const double G = 1;
    const double eps2 = 1e-12;


    // ===============================
    // Compute Forces + Sum Forces
    // ===============================
    for (int i = 0; i < N0; ++i) {
      if (!isAlive(nbs.bodies[i])) continue;


      for (int j = i + 1; j < N0; ++j) {
        if (!isAlive(nbs.bodies[j])) continue;


        const double dx = nbs.bodies[j].x[0] - nbs.bodies[i].x[0];
        const double dy = nbs.bodies[j].x[1] - nbs.bodies[i].x[1];
        const double dz = nbs.bodies[j].x[2] - nbs.bodies[i].x[2];


        const double r2 = dx*dx + dy*dy + dz*dz + eps2;
        const double invR = 1.0 / std::sqrt(r2);
        const double invR3 = invR * invR * invR;


        const double mi = nbs.bodies[i].mass;
        const double mj = nbs.bodies[j].mass;


        const double factor = G * mi * mj * invR3;


        const double fx = factor * dx;
        const double fy = factor * dy;
        const double fz = factor * dz;


        forceX[i] += fx; forceY[i] += fy; forceZ[i] += fz;
        forceX[j] -= fx; forceY[j] -= fy; forceZ[j] -= fz;
      }
    }


    // ===============================
    // Update velocities and Positions
    // ===============================
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


    // ===============================
    // Collision Handling
    // ===============================
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


            const double dist2 = dx*dx + dy*dy + dz*dz;
            const double thresh = C * (nbs.bodies[i].mass + nbs.bodies[j].mass);
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
        }
  }


  closePVDFile();
  std::cout << "Simulation completed.\n";


  return 0;
}