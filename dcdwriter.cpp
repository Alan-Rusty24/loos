/*
  dcdwriter.cpp
  (c) 2008 Tod D. Romo

  Grossfield Lab
  Department of Biochemistry and Biophysics
  University of Rochester Medical School

*/


#include <loos.hpp>
#include <dcdwriter.hpp>

void DCDWriter::writeF77Line(StreamWrapper& ofs, const char* const data, const unsigned int len) {
  DataOverlay d;

  d.ui = len;

  ofs()->write((char *)&d, sizeof(len));
  ofs()->write(data, len);
  ofs()->write((char *)&d, sizeof(len));
}


string DCDWriter::fixStringSize(const string& s, const unsigned int n) {
  string result(s);

  if (s.size() < n) {
    int i = n - s.size();
    result += string(" ", i);
  } else if (s.size() > n)
    result = s.substr(0, n);

  return(result);
}


void DCDWriter::writeHeader(void) {
  unsigned int icntrl[20];
  DataOverlay *dop = (DataOverlay *)(&icntrl[0]);
  unsigned int i;
  for (i=0; i<20; i++)
    icntrl[i] = 0;

  icntrl[0] = _nsteps;
  icntrl[1] = 1;
  icntrl[2] = 1;
  icntrl[3] = _nsteps;
  icntrl[7] = _natoms * 3 - 6;
  icntrl[8] = 0;
  dop[9].f = _timestep;
  icntrl[10] = _has_box;
  icntrl[19] = 27;

  writeF77Line(_ofs, (char *)dop, 20 * sizeof(unsigned int));

  unsigned int size = 4 + 80 * _titles.size();
  char *ptr = new char[size];
  unsigned int *iptr = (unsigned int *)ptr;
  *iptr = _titles.size();
  for (i=0; i<_titles.size(); i++) {
    string s = fixStringSize(_titles[i], 80);
    memcpy(ptr + 4 + 80*i, s.c_str(), 80);
  }
  writeF77Line(_ofs, ptr, size);
  delete[] ptr;
  

  dop[0].ui = _natoms;
  writeF77Line(_ofs, (char *)dop, 1 * sizeof(unsigned int));
}



void DCDWriter::writeBox(const GCoord& box) {
  double xtal[6] = { 1.0, box[0], 1.0, box[1], box[2], 1.0 };

  writeF77Line(_ofs, (char *)xtal, 6*sizeof(double));
}



void DCDWriter::writeFrame(const AtomicGroup& grp) {

  if (_current >= _nsteps)
    throw(runtime_error("Attempting to write more frames than requested."));

  if (grp.size() != _natoms)
    throw(runtime_error("Frame group atom count mismatch"));

  if (!_has_box && grp.isPeriodic())
    throw(runtime_error("Frame has periodic info but none was requested to be written to the DCD."));

  if (_has_box)
    writeBox(grp.periodicBox());

  float *data = new float[_natoms];
  int i;
  for (i=0; i<_natoms; i++)
    data[i] = grp[i]->coords().x();
  writeF77Line(_ofs, (char *)data, _natoms * sizeof(float));

  for (i=0; i<_natoms; i++)
    data[i] = grp[i]->coords().y();
  writeF77Line(_ofs, (char *)data, _natoms * sizeof(float));

  for (i=0; i<_natoms; i++)
    data[i] = grp[i]->coords().z();
  writeF77Line(_ofs, (char *)data, _natoms * sizeof(float));

  ++_current;
}


void DCDWriter::writeFrame(const vector<AtomicGroup>& grps) {
  vector<AtomicGroup>::const_iterator i;

  for (i= grps.begin(); i != grps.end(); i++)
    writeFrame(*i);
}

