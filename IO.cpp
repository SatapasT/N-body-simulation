#include "IO.h"

#include <iostream>
#include <fstream>
#include <sstream>

void writeVTKSnapshot(NBodySimulation nbs, int snapshotNumber) {
    std::ostringstream filename;
    filename << "paraview-output/result-" << snapshotNumber << ".vtp";
    std::ofstream out(filename.str());
    if (!out) {
        std::cerr << "Failed to open output file " << filename.str() << "\n";
        return;
    }

    out << "<VTKFile type=\"PolyData\">\n";
    out << "<PolyData>\n";
    // If you modify the data structures containing bodies, this line
    // needs to be modified
    out << " <Piece NumberOfPoints=\"" << nbs.bodies.size() << "\">\n";
    out << "  <Points>\n";
    out << "   <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">";

    // If you modify the data structures containing bodies, this for loop 
    // needs to be modified
    for (const auto& b : nbs.bodies) {
        out << b.x[0] << " " << b.x[1] << " " << b.x[2] << " ";
    }

    out << "</DataArray>\n";
    out << "  </Points>\n";
    out << " </Piece>\n";
    out << "</PolyData>\n";
    out << "</VTKFile>\n";

    out.close();
}

void openPVDFile() {
    std::ofstream pvdFile("paraview-output/result.pvd", std::ios::out);
    if (!pvdFile) {
        std::cerr << "Failed to open result.pvd for writing\n";
        return;
    }
    pvdFile << "<?xml version=\"1.0\"?>\n";
    pvdFile << "<VTKFile type=\"Collection\" version=\"0.1\" byte_order=\"LittleEndian\" compressor=\"vtkZLibDataCompressor\">\n";
    pvdFile << "<Collection>\n";
    pvdFile.close();
}

void addSnapshotToPVD(int snapshotNumber) {
    std::ofstream pvdFile("paraview-output/result.pvd", std::ios::app);
    if (!pvdFile) {
        std::cerr << "Failed to open result.pvd for appending\n";
        return;
    }
    pvdFile << "  <DataSet timestep=\"" << snapshotNumber
            << "\" group=\"\" part=\"0\" file=\"result-" << snapshotNumber << ".vtp\"/>\n";
    pvdFile.close();
}

void closePVDFile() {
    std::ofstream pvdFile("paraview-output/result.pvd", std::ios::app);
    if (!pvdFile) {
        std::cerr << "Failed to open result.pvd to close\n";
        return;
    }
    pvdFile << "</Collection>\n";
    pvdFile << "</VTKFile>\n";
    pvdFile.close();
}

