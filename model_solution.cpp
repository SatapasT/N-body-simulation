#include <iostream>
#include <cmath>
#include <vector>
#include "NBodySimulation.h"
#include "IO.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " input_file.txt\n";
        return 1;
    }
    NBodySimulation nbs(argv[1]);
    
    // Write initial snapshot
    openPVDFile();
    int snapshotCounter = 0;
    writeVTKSnapshot(nbs, snapshotCounter);
    addSnapshotToPVD(snapshotCounter);

    // Initialise simulation at t = 0
    double t = 0.0;
    double nextPlotTime = nbs.tPlotDelta;

   int N = (int)nbs.bodies.size();

    while (t < nbs.tFinal) {
        std::vector<double> forceX(N, 0.0);
        std::vector<double> forceY(N, 0.0);
        std::vector<double> forceZ(N, 0.0);

        // Compute gravitational forces
        for (int i = 0; i < N; ++i) {
            for (int j = i + 1; j < N; ++j) {
                double dx = nbs.bodies[j].x[0] - nbs.bodies[i].x[0];
                double dy = nbs.bodies[j].x[1] - nbs.bodies[i].x[1];
                double dz = nbs.bodies[j].x[2] - nbs.bodies[i].x[2];

                double distSqr = dx*dx + dy*dy + dz*dz;
                double dist = std::sqrt(distSqr);
                double distCubed = distSqr * dist;

                double F = (nbs.bodies[i].mass * nbs.bodies[j].mass) / distCubed;

                forceX[i] += F * dx;
                forceY[i] += F * dy;
                forceZ[i] += F * dz;

                forceX[j] -= F * dx;
                forceY[j] -= F * dy;
                forceZ[j] -= F * dz;
            }
        }

        // Update velocities and positions
        for (int i = 0; i < N; ++i) {
            nbs.bodies[i].v[0] += nbs.dt * forceX[i] / nbs.bodies[i].mass;
            nbs.bodies[i].v[1] += nbs.dt * forceY[i] / nbs.bodies[i].mass;
            nbs.bodies[i].v[2] += nbs.dt * forceZ[i] / nbs.bodies[i].mass;

            nbs.bodies[i].x[0] += nbs.dt * nbs.bodies[i].v[0];
            nbs.bodies[i].x[1] += nbs.dt * nbs.bodies[i].v[1];
            nbs.bodies[i].x[2] += nbs.dt * nbs.bodies[i].v[2];
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

