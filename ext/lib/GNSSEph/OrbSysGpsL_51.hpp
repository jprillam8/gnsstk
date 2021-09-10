//==============================================================================
//
//  This file is part of GNSSTk, the GNSS Toolkit.
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
//  Copyright 2004-2021, The Board of Regents of The University of Texas System
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

/**
 * @file OrbSysGpsL_51.hpp
 * Designed to support loading GPS LNAV SV Config and Health data
 * Subframe 5, Page 25, SV ID 51
 */

#ifndef SGLTK_ORBSYSGPSL_51_HPP
#define SGLTK_ORBSYSGPSL_51_HPP


#include <string>
#include <iostream>
#include <math.h>

#include "OrbSysGpsL.hpp"

namespace gnsstk
{
   class OrbSysGpsL_51 : public OrbSysGpsL
   {
   public:
   
         /// Default constructor
      OrbSysGpsL_51();
  
         /** Constructor for creating directly from a PackedNavBits object
          * @throw InvalidParameter
          */
      OrbSysGpsL_51(const PackedNavBits& msg);
      
         /// Destructor
      virtual ~OrbSysGpsL_51() {}

         /// Clone method
      virtual OrbSysGpsL_51* clone() const;
        
         /**
          * Store the contents of Subframe 5, Page 25 in this object.
          * @param msg - 300 bits of Subframe 5, Page 25.
          * @throw InvalidParameter if message data is invalid
          */
      virtual void loadData(const PackedNavBits& msg);
         
      virtual bool isSameData(const OrbData* right) const;      

      virtual std::string getName() const
      {
         return "Health";
      }

      virtual std::string getNameLong() const
      {
         return "GPS LNAV SV Health";
      }

         /** Output the contents of this orbit data to the given stream.
          * @throw InvalidRequest if the required data has not been stored.
          */
      virtual void dumpTerse(std::ostream& s = std::cout) const;

         /**
          * @throw InvalidRequest
          */
      virtual void dumpBody(std::ostream& s = std::cout) const;

      unsigned short WNa;
      unsigned long toa;
      CommonTime ctToa;

         // SV Health
         // See IS-GPS-200 Fig 20-1 Sheet 9 and 
         // Section 
      unsigned short health[25];  // Index 1-24 are used for PRN 1-14

         // Index of bit location in the message (for loadData())
      static const unsigned short hBits[];

   }; // end class OrbSysGpsL_51

} // end namespace gnsstk

#endif 

