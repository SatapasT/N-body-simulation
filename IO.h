#ifndef IO_H
#define IO_H

#include "NBodySimulation.h"


void writeVTKSnapshot(NBodySimulation nbs, int snapshotNumber);

void openPVDFile();
void addSnapshotToPVD(int snapshotNumber);
void closePVDFile();

#endif // IO_H

