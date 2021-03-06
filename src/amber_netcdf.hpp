// (c) 2012 Tod D. Romo, Grossfield Lab, URMC

#if !defined(LOOS_AMBER_NETCDF_HPP)
#define LOOS_AMBER_NETCDF_HPP


#include <istream>
#include <string>
#include <netcdf.h>

#include <loos_defs.hpp>
#include <Coord.hpp>
#include <Trajectory.hpp>
#include <exceptions.hpp>

#include <amber_traj.hpp>

namespace loos {


	//! Returns true if the file is a NetCDF file
	bool isFileNetCDF(const std::string& fname);





	namespace {


		// These classes handle the template specialization for
		// determining which nc_get_vara function to call depending on the
		// desired output type.  NetCDF will handle any necessary
		// conversion from the variable's native format.
		template<typename T>
		class VarTypeDecider {

			// This is private to keep arbitrary types from compiling
			static int read(const int id, const int var, const size_t* st, const size_t *co, T* ip) { return(0); }
		};



		// The following are the supported types (based on what GCoord
		// typically holds...

		template<> class VarTypeDecider<float> {
		public:
			static int read(const int id, const int var, const size_t* st, const size_t* co, float* ip) {
				return(nc_get_vara_float(id, var, st, co, ip));
			}
		};

		template<> class VarTypeDecider<double> {
		public:
			static int read(const int id, const int var, const size_t* st, const size_t* co, double* ip) {
				return(nc_get_vara_double(id, var, st, co, ip));
			}
		};


	}



	//! Class for reading Amber Trajectories in NetCDF format
	class AmberNetcdf : public Trajectory {
	public:


		// Note: we don't call the base class constructor because we need
		// to keep it from trying to use an istream (since the C netcdf API
		// doesn't support this)

		explicit AmberNetcdf(const std::string& s, const uint na)
			: Trajectory(s),
			  _coord_data(new GCoord::element_type[na*3]),
			  _velocity_data(new GCoord::element_type[na*3]),
			  _box_data(new GCoord::element_type[3]),
			  _periodic(false),
			  _velocities(false),
			  _timestep(1e-12)
		{
			cached_first = false;
			init(s.c_str(), na);
		}


		~AmberNetcdf() {
			// ignore the return code since throwing in destructors is bad...
			nc_close(_ncid);

			delete[] _coord_data;
			delete[] _box_data;
		}

		std::string description() const { return("Amber trajectory (netCDF)"); }
		static pTraj create(const std::string& fname, const AtomicGroup& model) {
			if (isFileNetCDF(fname))
				return(pTraj(new AmberNetcdf(fname, model.size())));

			return(pTraj(new AmberTraj(fname, model.size())));
		}

		uint natoms() const { return(_natoms); }
		uint nframes() const { return(_nframes); }
		float timestep() const { return(_timestep); }

		bool hasPeriodicBox() const { return(_periodic); }
		GCoord periodicBox() const { return(GCoord(_box_data[0], _box_data[1], _box_data[2])); }

		virtual bool hasVelocities() const { return(_velocities); }
		virtual double velocityConversionFactor() const { return(1.0); }


		std::vector<GCoord> coords() const {
			std::vector<GCoord> res;
			for (uint i=0; i<_natoms; i += 3)
				res.push_back(GCoord(_coord_data[i], _coord_data[i+1], _coord_data[i+2]));
			return(res);
		}



	private:



		void init(const char* name, const uint natoms);
		void readGlobalAttributes();
		std::string readGlobalAttribute(const std::string& name);
		void readRawFrame(const uint frameno);

		void updateGroupCoordsImpl(AtomicGroup& g);
		void updateGroupVelocitiesImpl(AtomicGroup& g);
		bool parseFrame();
		void seekNextFrameImpl() { }
		void seekFrameImpl(const uint frame) { }
		void rewindImpl() { }

		std::vector<GCoord> velocitiesImpl() const;


	private:
		GCoord::element_type* _coord_data;
		GCoord::element_type* _velocity_data;
		GCoord::element_type* _box_data;
		bool _periodic;
		bool _velocities;
		float _timestep;
		int _ncid;
		size_t _nframes;
		size_t _natoms;
		int _coord_id;
		size_t _coord_size;
		int _cell_lengths_id;
		int _velocities_id;
		std::string _title, _application, _program, _programVersion, _conventions, _conventionVersion;
	};


}



#endif
