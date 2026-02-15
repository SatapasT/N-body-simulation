#ifndef NBODY_SIMULATION_H
#define NBODY_SIMULATION_H

#include <vector>
#include <string>
#include <fstream>
#include <iostream>

struct Body {
    double x[3];
    double v[3];
    double mass;
};

class NBodySimulation {
public:
    double tPlotDelta;
    double tFinal;
    double dt;

    // If you exchange this data structure, make sure to update the constructor as well
    // If need be add a destructor to the class
    std::vector<Body> bodies;

    NBodySimulation() 
	    : tPlotDelta(0.01), tFinal(1.0), dt(0.001), bodies() {
		    // If no input is given use a simple 3 body problem
		    Body b;
		    b.x[0] = 0.0; b.x[1] = 0.0; b.x[2] = 0.0;
		    b.v[0] = 1.0; b.v[1] = 0.0; b.v[2] = 0.0;
		    b.mass = 1.0;
		    bodies.push_back(b);

		    b.x[0] = 1.0; b.x[1] = 0.0; b.x[2] = 0.0;
		    b.v[0] = 0.0; b.v[1] = 1.0; b.v[2] = 0.0;
		    b.mass = 1.0;
		    bodies.push_back(b);

		    b.x[0] = -1.0; b.x[1] = 0.0; b.x[2] = 0.0;
		    b.v[0] = 0.0; b.v[1] = -1.0; b.v[2] = 0.0;
		    b.mass = 1.0;
		    bodies.push_back(b);
    }

    explicit NBodySimulation(const std::string& filename)
	    : tPlotDelta(0.0), tFinal(0.0), dt(0.0), bodies() {
		    std::ifstream infile(filename);
		    if (!infile) {
			    std::cerr << "Cannot open input file.\n";
		    }

		    infile >> tPlotDelta >> tFinal >> dt;

		    Body b;
		    while (infile >> b.x[0] >> b.x[1] >> b.x[2]
				    >> b.v[0] >> b.v[1] >> b.v[2]
				    >> b.mass) {
			    if (b.mass <= 0.0) {
				    std::cerr << "Invalid mass encountered.\n";
			    }
			    bodies.push_back(b);
		    }
	    }
};

#endif // NBODY_SIMULATION_H

