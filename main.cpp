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
	   // TODO: Your code for the N-body simulation goes here
    	   // You can use the data structures in NBodysolver.h or modify them (consider AoS vs. SoA)
    	   // The reading of the input and writing of the output in IO.cpp may need to be modified if the data structures are
    	   // Do not modify the input or output file format. It must remain the same! 
	   t += nbs.dt;

	   if (t >= nextPlotTime) {
		   snapshotCounter++;
		   writeVTKSnapshot(nbs, snapshotCounter);
		   addSnapshotToPVD(snapshotCounter);
		   nextPlotTime += nbs.tPlotDelta;
		   std::cout << "Plot next snapshot"
			   << ",\t t="         << t
			   << ",\t dt="        << nbs.dt
			   << ",\t N="         << nbs.bodies.size()
			   << std::endl;
		   // In addition to the above quantities you may want to track maximum velocity 
		   // and smallest distance between masses. This is particularly useful when implementing collisions.
	   }
   }

   closePVDFile();
   std::cout << "Simulation completed.\n";

   return 0;
}

