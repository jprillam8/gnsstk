//==============================================================================
//
//  This file is part of GNSSTk, the ARL:UT GNSS Toolkit.
//
//  The GNSSTk is free software; you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation; either version 3.0 of the License, or
//  any later version.
//
//  The GNSSTk is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with GNSSTk; if not, write to the Free Software Foundation,
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
//
//  This software was developed by Applied Research Laboratories at the
//  University of Texas at Austin.
//  Copyright 2004-2022, The Board of Regents of The University of Texas System
//
//==============================================================================

//==============================================================================
//
//  This software was developed by Applied Research Laboratories at the
//  University of Texas at Austin, under contract to an agency or agencies
//  within the U.S. Department of Defense. The U.S. Government retains all
//  rights to use, duplicate, distribute, disclose, or release this software.
//
//  Pursuant to DoD Directive 523024
//
//  DISTRIBUTION STATEMENT A: This software has been approved for public
//                            release, distribution is unlimited.
//
//==============================================================================

/** @file PreciseRange.cpp
    Implement computation of range and associated quantities from NavLibrary,
    given receiver position and time.
*/

// system includes
#include <sstream> // for ostringstream
#include <tuple>
// GNSSTk includes
#include "GNSSconstants.hpp"
#include "EllipsoidModel.hpp"
#include "GPSEllipsoid.hpp"
#include "MiscMath.hpp"
#include "Xvt.hpp"
#include "RawRange.hpp"

// geomatics
#include "SolarPosition.hpp"
#include "SunEarthSatGeometry.hpp"

#include "PreciseRange.hpp"

using namespace std;

namespace gnsstk
{
   double PreciseRange::ComputeAtTransmitTime(const CommonTime& nomRecTime,
                                              const double pr,
                                              const Position& rxPos,
                                              const SatID sat,
                                              const AntexData& antenna,
                                              const std::string& freq1,
                                              const std::string& freq2,
                                              SolarSystem& solSys,
                                              NavLibrary& eph,
                                              bool isCOM,
                                              const EllipsoidModel& ellipsoid)
   {
      try
      {
         bool success;
         Xvt svPosVel;

            // Initial transmit time estimate from pseudorange
         std::tie(success, transmit) =
            RawRange::estTransmitFromObs(nomRecTime, pr, eph,
                                         NavSatelliteID(sat));
         if(!success)
         {
            InvalidRequest ir("getXvt failed");
            GNSSTK_THROW(ir);
         }

         if(!eph.getXvt(NavSatelliteID(sat), transmit, svPosVel))
         {
            InvalidRequest ir("getXvt failed");
            GNSSTK_THROW(ir);
         }

         SatR.setECEF(svPosVel.x);

            /* Sagnac effect
               ref. Ashby and Spilker, GPS: Theory and Application, 1996 Vol 1, pg
               673. this is w(Earth) * (SatR cross rxPos).Z() / c*c in seconds beware
               numerical error by differencing very large to get very small */
         Sagnac = ((SatR.X() / ellipsoid.c()) * (rxPos.Y() / ellipsoid.c()) -
                   (SatR.Y() / ellipsoid.c()) * (rxPos.X() / ellipsoid.c())) *
                  ellipsoid.angVelocity();
         transmit -= Sagnac;

            /* compute other delays -- very small
               2GM/c^2 = 0.00887005608 m^3/s^2 * s^2/m^2 = m */
         double rx = rxPos.radius();
         if (::fabs(rx) < 1.e-8)
         {
            GNSSTK_THROW(Exception("Rx at origin!"));
         }
         double rs   = SatR.radius();
         double dr;
         std::tie(dr, std::ignore) =
            RawRange::computeRange(rxPos, SatR, 0, ellipsoid);
         relativity2 = -0.00887005608 * ::log((rx + rs + dr) / (rx + rs - dr));
         transmit -= relativity2 / ellipsoid.c();

            // iterate satellite position
         if(! eph.getXvt(NavSatelliteID(sat), transmit, svPosVel))
         {
            InvalidRequest ir("getXvt failed");
            GNSSTK_THROW(ir);
         }

            // ----------------------------------------------------------
            // save relativity and satellite clock
         relativity  = svPosVel.relcorr * ellipsoid.c();
         satclkbias  = svPosVel.clkbias * ellipsoid.c();
         satclkdrift = svPosVel.clkdrift * ellipsoid.c();

            // correct for Earth rotation
         std::tie(rawrange, std::ignore) =
            RawRange::computeRange(rxPos, SatR, 0, ellipsoid);
         double dt = rawrange / ellipsoid.c();
         std::tie(rawrange, svPosVel) =
            RawRange::computeRange(rxPos, svPosVel, dt, ellipsoid);

            // Do NOT replace these with Xvt
         SatR.setECEF(svPosVel.x);
         SatV.setECEF(svPosVel.v);

            // Compute line of sight, satellite to receiver
         Triple S2R(rxPos.X() - SatR.X(), rxPos.Y() - SatR.Y(), rxPos.Z() - SatR.Z());
         S2R = S2R.unitVector();

            // ----------------------------------------------------------
            // satellite antenna pco and pcv
         if (isCOM && antenna.isValid())
         {
               // must combine PCO/V from freq1,2
            unsigned int freq1num(strtoul(freq1.substr(1).c_str(), 0, 10));
            unsigned int freq2num(strtoul(freq2.substr(1).c_str(), 0, 10));
            double alpha(getAlpha(sat.system, freq1num, freq2num));
            double fact1((alpha + 1.0) / alpha);
            double fact2(-1.0 / alpha);

               // if single frequency, freq2==alpha==0, fact's=nan
            if (freq2num == 0)
            {
               fact1 = 1.0;
               fact2 = 0.0;
            }

               // rotation matrix from satellite attitude: Rot*[XYZ]=[body frame]
            Matrix<double> SVAtt;

               // get satellite attitude from SolarSystem; if not valid, get low
               // accuracy version from SunEarthSatGeometry and SolarPosition.
            if (solSys.EphNumber() != -1)
            {
               SVAtt = solSys.satelliteAttitude(transmit, SatR);
            }
            else
            {
               double AR; // angular radius of sun
               Position Sun = solarPosition(transmit, AR);
               SVAtt        = satelliteAttitude(SatR, Sun);
            }

               // phase center offset vector in body frame
            Vector<double> PCO(3);
            Triple pco2, pco1(antenna.getPhaseCenterOffset(freq1));
            if (freq2num != 0)
            {
               pco2 = antenna.getPhaseCenterOffset(freq2);
            }
            for (unsigned int i = 0; i < 3; i++)
            {
                  // body frame, mm -> m, iono-free combo
               PCO(i) = (fact1 * pco1[i] + fact2 * pco2[i]) / 1000.0;
            }

               // PCO vector (from COM to PC) in ECEF XYZ frame, m
            SatPCOXYZ = transpose(SVAtt) * PCO;

            Triple pcoxyz(SatPCOXYZ(0), SatPCOXYZ(1), SatPCOXYZ(2));
               // line of sight phase center offset
            satLOSPCO = pcoxyz.dot(S2R); // meters

               // phase center variation TD should this should be subtracted from
               // rawrange? get the body frame azimuth and nadir angles
            double nadir, az, pcv1, pcv2(0.0);
            satelliteNadirAzimuthAngles(SatR, rxPos, SVAtt, nadir, az);
            pcv1 = antenna.getPhaseCenterVariation(freq1, az, nadir);
            if (freq2num != 0)
            {
               pcv2 = antenna.getPhaseCenterVariation(freq2, az, nadir);
            }
            satLOSPCV = 0.001 * (fact1 * pcv1 + fact2 * pcv2);
         }
         else
         {
            satLOSPCO = satLOSPCV = 0.0;
            SatPCOXYZ             = Vector<double>(3, 0.0);
         }

            // ----------------------------------------------------------
            // direction cosines
         for (unsigned int i = 0; i < 3; i++)
         {
            cosines[i] = -S2R[i]; // receiver to satellite
         }

            // elevation and azimuth
         elevation         = rxPos.elevation(SatR);
         azimuth           = rxPos.azimuth(SatR);
         elevationGeodetic = rxPos.elevationGeodetic(SatR);
         azimuthGeodetic   = rxPos.azimuthGeodetic(SatR);

            // return corrected ephemeris range
         return (rawrange - satclkbias - relativity - relativity2 - satLOSPCO +
                 satLOSPCV);

      } // end try
      catch (gnsstk::Exception& e)
      {
         GNSSTK_RETHROW(e);
      }

   } // end PreciseRange::ComputeAtTransmitTime

} // namespace gnsstk
