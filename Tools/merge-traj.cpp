/*
  merge-traj: combine multiple trajectories into a single long trajectory.  
  If the target trajectory exists, append to it.

  Alan Grossfield
  Grossfield Lab
  Department of Biochemistry and Biophysics
  University of Rochester Medical School
 
  This file is part of LOOS.

  LOOS (Lightweight Object-Oriented Structure library)
  Copyright (c) 2008, Alan Grossfield
  Department of Biochemistry and Biophysics
  School of Medicine & Dentistry, University of Rochester

  This package (LOOS) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation under version 3 of the License.

  This package is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <loos.hpp>

using namespace std;

using namespace loos;

namespace opts = loos::OptionsFramework;
namespace po = loos::OptionsFramework::po;

// global for parsing program options
string model_name, output_traj, output_traj_downsample;
string center_selection;
string xy_center_selection;
string z_center_selection;
vector<string> input_dcd_list;
int downsample_rate;
bool skip_first_frame=false;
bool reimage_by_molecule=false;
bool selection_split=false;


// @cond TOOLS_INTERNAL
class ToolOptions : public opts::OptionsPackage
{
public:

  void addGeneric(po::options_description& o)
  {
    o.add_options()
      ("downsample-dcd", po::value<string>(&output_traj_downsample),
       "Downsampled DCD, must be synced with output_traj")
      ("downsample-rate", po::value<int>(&downsample_rate)->default_value(10),
       "Write every nth frame to downsampled DCD")
      ("centering-selection", po::value<string>(&center_selection)->default_value(""), "Selection for centering")
      ("xy-centering-selection", po::value<string>(&xy_center_selection)->default_value(""), "Selection for centering in xy-plane")
      ("z-centering-selection", po::value<string>(&z_center_selection)->default_value(""), "Selection for centering along z")
      ("selection-is-split", po::value<bool>(&selection_split)->default_value(false), "Selection is split across image boundaries")
      ("skip-first-frame", po::value<bool>(&skip_first_frame)->default_value(false), "Skip first frame of each trajectory (for xtc files)")
      ("fix-imaging", po::value<bool>(&reimage_by_molecule)->default_value(false), "Reimage the system so molecules aren't broken across image boundaries")
      ("sort", po::value<bool>(&sort_flag)->default_value(false), "Sort (numerically) the input DCD files.")
      ("scanf", po::value<string>(&scanf_spec)->default_value(""), "Sort using a scanf-style format string")
      ("regex", po::value<string>(&regex_spec)->default_value("(\\d+)\\D*$"), "Sort using a regular expression")

      ;
  }

  bool postConditions(po::variables_map& map)
      {
      cerr << "center: " << center_selection
           << " xy: " << xy_center_selection
           << " z: " << z_center_selection
           << endl;
      if ( (center_selection.length() != 0) &&
           ( (xy_center_selection.length() !=0) || (z_center_selection.length() !=0)) )
          {
          cerr << "Can't specify both centering-selection and either xy-centering-selection or z-centering-selection" 
              << endl;
          return(false);
          }
      return(true);
      }

  string print() const
    {
    ostringstream oss;

    oss << boost::format("downsample-dcd='%s', downsample-rate=%d, centering-selection='%s', skip-first-frame=%d, fix-imaging=%d")
      % output_traj_downsample
      % downsample_rate
      % center_selection
      % skip_first_frame
      % reimage_by_molecule;

    return(oss.str());
    }

  bool sort_flag;
  string scanf_spec, regex_spec;
};

// @endcond

// TODO: need to fix the docs to reflect xy-centering and z-centering
string fullHelpMessage(void)
    {
    string s = 
   "\n"
   "SYNOPSIS\n"
   "\n"
   "Merge and downsample a set of trajectory files into a single file.\n"
   "\n"
   "DESCRIPTION\n"
   "\n"
"This program takes a set of trajectory files in any of the formats\n"
"supported by LOOS and efficiently produces a merged trajectory in\n"
"DCD format.  It can also produce a second, downsampled trajectory,\n"
"and can recenter and reimage the coordinates at the same time.\n"
"\n"
"Unlike other tools, such as catdcd, merge-traj works by appending to\n"
"existing trajectory files instead of rewriting them from scratch each\n"
"time.  This can dramatically improve the performance in a common usage\n"
"case, where a set of trajectories is generated over a period of days\n"
"or weeks, and merge-traj is used to create a daily merge of the data\n"
"available to date.  \n"
"\n"
"The user specifies the target for merged trajectory, and a list of\n"
"trajectory files to be merged.  The program determines the number of \n"
"frames in the current merged trajectory, and walks through the list\n"
"of trajectories to be merged, skipping that number of frames and only\n"
"then beginning to append new frames.  This means that a) the user\n"
"must specify the trajectories in the correct order, and b) all \n"
"trajectories must be specified each time (not just the newest files).\n"
"merge-traj correctly handles the case where one of the trajectories \n"
"to be merged has grown since the previous merge.\n"
"\n"
"Options related to downsampling\n"
"\n"
"--downsample-dcd     a second merged DCD file, with frames written at\n"
"                     lower frequency\n"
"--downsample-rate    integer specifying how often to write to the \n"
"                     downsampled DCD file, e.g. 10 means write every\n"
"                     10th frame\n"
"Note: the downsampled DCD file must be synchronized with the fully sampled\n"
"one.  This is the user's responsibility, as the code doesn't do any \n"
"additional checking.  The easiest way is to put the command line into\n"
"a script to ensure that both files are always used.\n"
"\n"
"Options related to recentering\n"
"\n"
"It is often convenient to clean up the trajectory at merge time, removing\n"
"center of mass motion for some component of the system (e.g. the protein).\n"
"Accordingly, merge-traj has the following options\n"
"\n"
" --centering-selection     the centroid of the atoms specificed by the \n"
"                           selection string will be moved to the origin in\n"
"                           each frame.  No rotations are performed.\n"
" --xy-centering-selection  same as --centering-selection, except only move in\n"
"                           the xy plane.  Can't be used with --centering-selection\n"
"                           but can be combined with --z-centering-selection\n"
" --z-centering-selection   same as --centering-selection, except only move in\n"
"                           the z direction.  Can't be used with --centering-selection\n"
"                           but can be combined with --xy-centering-selection\n"
" --selection-is-split      This flag indicates that the selection specified\n"
"                           by --centering-selection may be split across image\n"
"                           boundaries, in which case the centroid can be far\n"
"                           from where the atoms are actually located.  In \n"
"                           this case, the recentering is performed in 2 \n"
"                           stages, first putting the selection into a \n"
"                           single image, then recentering.  Works correctly with\n"
"                           all 3 centering variants\n"
" --fix-imaging             Ensure that molecules are not broken across \n"
"                           image boundaries.  This is generally necessary\n"
"                           for simulations in GROMACS.\n"
"\n"
"\n"
"In addition, for merging GROMACS XTC files there is an additional flag:\n"
"\n"
"--skip-first-frame         XTC files can contain the initial structure as\n"
"                           the first frame.  In this case, use this flag to\n"
"                           prevent duplication upon merging.\n"
"\n"
"\n"
"EXAMPLE\n"
"\n"
"\n"
"Here is an example command line:\n"
"\n"
"merge-traj --centering-selection 'segid==\"OPSN\"' --downsample-dcd merged_1ns.dcd \\\n"
"  --downsample-rate 10 start.psf merged.dcd  traj.[0-9].dcd  \\\n"
"  traj.[1-9][0-9].dcd traj.[1-9][0-9][0-9].dcd\n"
"\n"
"This will merge a set of trajectory files named traj.0.dcd, traj.1.dcd, \n"
"etc., going up to hundreds of trajectory files as necessary (this is \n"
"tcsh, but bash would be similar).  It's necessary to specify the merge \n"
"this way in order to get the files in the proper order on the command \n"
"line.  start.psf is the system file, merged.dcd is the target for the\n"
"full-resolution merged trajectory.  A second merged trajectory, \n"
"merged_1ns.dcd, will also be created, containing only every 10th frame.\n"
"On each frame the full system will be translated and reimaged \n"
"such that segid \"OPSN\" is at the origin.  \n"
"\n"
"\n"
"NOTE: This code will work best if the system file has connectivity information.\n"
"When this information is present, it is used to split the system into \n"
"individual molecules; when absent, it falls back to using the segment name.\n"
"This can lead to unintended results for segments that are made of many\n"
"individual atoms (e.g. ions in solution), causing them to end up outside the \n"
"box.\n"
"\n";

    return (s);
    }





// Code required for parsing trajectory filenames... (liberated from subsetter)

struct ScanfFmt
{
  ScanfFmt(const string& s) : fmt(s) { }

  uint operator()(const string& s) const
    {
    uint d;
    if (sscanf(s.c_str(), fmt.c_str(), &d) != 1)
        {
        cerr << boost::format("Bad conversion of '%s' using format '%s'\n") % s % fmt;
        exit(-20);
        }

    return(d);
    }

  string fmt;
};



struct RegexFmt
{
    RegexFmt(const string& s) : fmt(s)
    {
    regexp = boost::regex(s, boost::regex::perl);
    }
  
    uint operator()(const string& s) const
    {
        boost::smatch what;
        if (boost::regex_search(s, what, regexp))
            {
            for (uint i=0; i<what.size(); ++i)
                    {
                    string submatch(what.str(i));
                    char *p;
                    if (submatch.empty())    // Should never happen?
                        continue;

                    uint val = strtoul(submatch.c_str(), &p, 10);
                    if (*p == '\0')
                        return(val);
                    }
            }
    
        cerr << boost::format("Bad conversion of '%s' using regexp '%s'\n") % s % fmt;
        exit(-20);
    }

  string fmt;
  boost::regex regexp;
};



// Binding of trajectory name to the file # for sorting...
struct SortDatum 
{
    SortDatum(const string& s_, const uint n_) : s(s_), n(n_) { }
    SortDatum() : s(""), n(0) { }

    string s;
    uint n;
};


// Define comparison function for sort
bool operator<(const SortDatum& a, const SortDatum& b)
{
    return(a.n < b.n);
}



// Given a vector of trajectory filenames, along with a functor for
// extracting the frame index from the filename, sorts it in numeric
// ascending order...

template<class FmtOp>
vector<string> sortNamesByFormat(vector<string>& names, const FmtOp& op)
{
    uint n = names.size();
    vector<SortDatum> bound(n);
    for (uint i=0; i<n; ++i)
        {
        bound[i] = SortDatum(names[i], op(names[i]));
        }
  
    sort(bound.begin(), bound.end());

    vector<string> sorted(n);
    for (uint i=0; i<n; ++i)
        sorted[i] = bound[i].s;

    return(sorted);
}



int main(int argc, char *argv[])
{
    string hdr = invocationHeader(argc, argv);
    opts::BasicOptions* bopts = new opts::BasicOptions(fullHelpMessage());
    ToolOptions* topts = new ToolOptions;
    opts::RequiredArguments* ropts = new opts::RequiredArguments;
    ropts->addArgument("model", "model-filename");
    ropts->addArgument("output_traj", "output-trajectory");
    ropts->addVariableArguments("input_traj", "trajectory");

    opts::AggregateOptions options;
    options.add(bopts).add(topts).add(ropts);
    if (!options.parse(argc, argv))
      exit(-1);

    model_name = ropts->value("model");
    output_traj = ropts->value("output_traj");
    input_dcd_list = ropts->variableValues("input_traj");

    if (topts->sort_flag)
        {
        if (!topts->scanf_spec.empty())
            {
            input_dcd_list = sortNamesByFormat(input_dcd_list, ScanfFmt(topts->scanf_spec));
            } 
        else
            {
            input_dcd_list = sortNamesByFormat(input_dcd_list, RegexFmt(topts->regex_spec));
            }
        }
    
    cout << hdr << endl;
    AtomicGroup system = createSystem(model_name);

    // We check for specifying both xy/z and full in the code
    // that processes the command line options, so we don't
    // have to do it here
    bool full_recenter = false;
    bool xy_recenter = false;
    bool z_recenter = false;
    if ( center_selection.length() != 0 )
        {
        full_recenter = true;
        }
    if ( xy_center_selection.length() != 0 )
        {
        xy_recenter = true;
        }
    if ( z_center_selection.length() != 0 )
        {
        z_recenter = true;
        }

    pTrajectoryWriter output = createOutputTrajectory(output_traj, true);

    pTrajectoryWriter output_downsample;
    bool do_downsample = (output_traj_downsample.length() > 0);
    if (do_downsample)
        {
        output_downsample = createOutputTrajectory(output_traj_downsample, true);
        }

    // Set up to do the recentering
    vector<AtomicGroup> molecules;
    AtomicGroup center, xy_center, z_center;
    vector<AtomicGroup>::iterator m;
    if ( full_recenter )
        {
        center = selectAtoms(system, center_selection);
        }
    else
        {
        if ( xy_recenter )
            {
            xy_center = selectAtoms(system, xy_center_selection);
            }
        if ( z_recenter )
            {
            z_center = selectAtoms(system, z_center_selection);
            }
        }

    if ( full_recenter || xy_recenter || z_recenter || reimage_by_molecule )
        {
        if ( system.hasBonds() )
            {
            molecules = system.splitByMolecule();
            }
        else
            {
            molecules = system.splitByUniqueSegid();
            }
        }

    uint original_num_frames = output->framesWritten();
    cout << "Target trajectory " 
         << output_traj
         << " has " 
         << original_num_frames
         << " frames."
         << endl;

    uint previous_frames = 0;
    vector<string>::iterator f;
    for (f=input_dcd_list.begin(); f!=input_dcd_list.end(); ++f)
        {
        pTraj traj=createTrajectory(*f, system);
        int nframes = traj->nframes();
        if (skip_first_frame && nframes > 1)
            {
            nframes--;
            }
        cout << "File: " << *f << ": " << nframes;

        if ( previous_frames + nframes <= original_num_frames) 
            // all of this file is contained in the existing file, skip it
            {
            // increment the frame pointer
            previous_frames += nframes;
            cout << " ( " << previous_frames << " )"
                 << "\tSkipping trajectory " 
                 << endl;

            }
        else
            // we need at least some of the data from this file
            {
            int frames_to_skip = original_num_frames - previous_frames;
            if ( frames_to_skip > 0 )
                {
                traj->seekFrame(frames_to_skip-1);
                }
            else
                {
                frames_to_skip = 0;
                }

            previous_frames += frames_to_skip;

            // if this is an xtc file, we need to skip 1 more frame
            if (skip_first_frame)
                {
                traj->readFrame();
                }

            cout << " ( " << previous_frames + nframes - frames_to_skip
                 << " ) "
                 << "\t Writing " << nframes - frames_to_skip 
                 << " frames."
                 << endl;

            while ( traj->readFrame() )
                {
                traj->updateGroupCoords(system);

                // Find the smallest box dimension
                GCoord box = system.periodicBox();
                double smallest=1e20;
                for (int i=0; i<3; i++)
                    {
                    if (box[i] < smallest)
                        {
                        smallest = box[i];
                        }
                    }

                smallest /=2.0;


                // If molecules can be broken across image bondaries
                // (eg GROMACS), then we may need 2 translations to 
                // fix them -- first, translate the whole molecule such 
                // that a single atom is at the origin, reimage the
                // molecule, and put it back
                if (reimage_by_molecule)
                    {
                    for (m=molecules.begin(); m != molecules.end(); ++m )
                        {
                        // This is relatively slow, so we'll skip the 
                        // cases we know we won't need this -- 1 particle
                        // molecules and molecules with small radii 
                        // Note: radius(true) computes the max distance between atom 0
                        //       and all other atoms in the group.  In certain perverse
                        //       cases the centroid can be closer than 1/2 box to all atoms
                        //       even when the molecule is split.
                        if ( (m->size() > 1) && (m->radius(true) > smallest) )
                            {
                            m->mergeImage();
                            m->reimage();
                            }
                        }
                    }


                if ( full_recenter || xy_recenter || z_recenter)
                    {
                    // If the selection is split, then we effectively need to 
                    // do the centering twice.  First, we pick one atom from the
                    // centering selection, translate the entire system so it's
                    // at the origin, and reimage.  This will get the selection
                    // region to not be split on the image boundary.  At that 
                    // point, we can just do regular imaging.
                    if (selection_split)
                        {
                        GCoord centroid;
                        if (full_recenter)
                            {
                            centroid = center[0]->coords();
                            }
                        else
                            {
                            if (xy_recenter)
                                {
                                centroid.x() = xy_center[0]->coords().x();
                                centroid.y() = xy_center[0]->coords().y();
                                }
                            if (z_recenter)
                                {
                                centroid.z() = z_center[0]->coords().z();
                                }
                            }

                        system.translate(-centroid);

                        for (m=molecules.begin(); m!=molecules.end(); m++)
                            {
                            m->reimage();
                            }
                        }
                    // Now, do the regular imaging.  Put the system centroid 
                    // at the origin, and reimage by molecule
                    GCoord centroid;
                    if (full_recenter)
                        {
                        centroid = center.centroid();
                        }
                    else
                        {
                        if (xy_recenter)
                            {
                            centroid = xy_center.centroid();
                            centroid.z() = 0.0;
                            }
                        if (z_recenter)
                            {
                            centroid.z() = z_center.centroid().z();
                            }
                        }
                    system.translate(-centroid);

                    for (m=molecules.begin(); m != molecules.end(); ++m )
                        {
                        m->reimage();
                        }

                    // Sometimes if the box has drifted enough, reimaging by molecule
                    // will significantly alter the centroid of the selected system, so
                    // we need to center a second time, which perversely means we'll need
                    // to reimage again. In my tests, this second go around is 
                    // necessary and sufficient to fix everything, but I'm willing 
                    // to be proved wrong.

                    centroid.zero();
                    if (full_recenter)
                        {
                        centroid = center.centroid();
                        }
                    else
                        {
                        if (xy_recenter)
                            {
                            centroid = xy_center.centroid();
                            centroid.z() = 0.0;
                            }
                        if (z_recenter)
                            {
                            centroid.z() = z_center.centroid().z();
                            }
                        }
                    system.translate(-centroid);

                    for (m=molecules.begin(); m != molecules.end(); ++m )
                        {
                        m->reimage();
                        }
#if DEBUG
                    cerr << "centroid after reimaging: " << centroid << endl;
#endif

                    system.translate(-centroid);

#if DEBUG
                    centroid = center.centroid();
                    cerr << "centroid after second reimaging: " << centroid << endl;
#endif 
                    }

                output->writeFrame(system);
                if ( do_downsample && (previous_frames % downsample_rate == 0) )
                    {
                    output_downsample->writeFrame(system);
                    }
                previous_frames++;
                }
            }

        }



    }
         
