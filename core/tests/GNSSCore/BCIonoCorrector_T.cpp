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

#include "TestUtil.hpp"
#include "BCIonoCorrector.hpp"
#include "NavLibrary.hpp"
#include "MultiFormatNavDataFactory.hpp"
#include "CivilTime.hpp"

namespace gnsstk
{
   std::ostream& operator<<(std::ostream& s, CorrectorType t)
   {
      s << StringUtils::asString(t);
      return s;
   }
}

class BCIonoCorrector_T
{
public:
   BCIonoCorrector_T();
   unsigned constructorTest();
   unsigned getCorrTestPosition();
   unsigned getCorrTestXvt();
   std::string dataPath;
};


BCIonoCorrector_T ::
BCIonoCorrector_T()
{
   dataPath = gnsstk::getPathData() + gnsstk::getFileSep();
}


unsigned BCIonoCorrector_T ::
constructorTest()
{
   TUDEF("BCIonoCorrector", "BCIonoCorrector");
   gnsstk::NavLibrary nl;
   gnsstk::BCIonoCorrector uut(nl);
   TUASSERTE(gnsstk::CorrectorType, gnsstk::CorrectorType::Iono, uut.corrType);
   TUASSERTE(gnsstk::NavLibrary*, &nl, &uut.navLib);
   TURETURN();
}


unsigned BCIonoCorrector_T ::
getCorrTestPosition()
{
   TUDEF("BCIonoCorrector", "getCorr(Position)");
   gnsstk::NavLibrary navLib;
   gnsstk::BCIonoCorrector uut(navLib);
   gnsstk::NavDataFactoryPtr ndf;
   gnsstk::Position stnPos(-740290.01, -5457071.705, 3207245.599);
   gnsstk::Position svPos(-16208820.579, -207275.833, 21038422.516);
   gnsstk::CommonTime when(gnsstk::CivilTime(2006,10,1,4,30,0,
                                             gnsstk::TimeSystem::Any));
   gnsstk::SatID sat(27, gnsstk::SatelliteSystem::GPS);
   gnsstk::ObsID oid(gnsstk::ObservationType::Phase, gnsstk::CarrierBand::L1,
                     gnsstk::TrackingCode::CA);
   gnsstk::NavType nav = gnsstk::NavType::GPSLNAV;
   ndf = std::make_shared<gnsstk::MultiFormatNavDataFactory>();
   gnsstk::MultiFormatNavDataFactory *mfndf =
      dynamic_cast<gnsstk::MultiFormatNavDataFactory*>(ndf.get());
   TUASSERTE(bool, true, mfndf->addDataSource(dataPath + "mixed.06n"));
   TUCATCH(navLib.addFactory(ndf));
   double corr = 0.0;
   TUASSERTE(bool, true, uut.getCorr(stnPos, svPos, sat, oid, when, nav, corr));
   TUASSERTFE(3.623343027333741, corr);
   TURETURN();
}


unsigned BCIonoCorrector_T ::
getCorrTestXvt()
{
   TUDEF("BCIonoCorrector", "getCorr(Xvt)");
   gnsstk::NavLibrary navLib;
   gnsstk::BCIonoCorrector uut(navLib);
   gnsstk::NavDataFactoryPtr ndf;
   gnsstk::Position stnPos(-740290.01, -5457071.705, 3207245.599);
   gnsstk::Xvt svPos;
   svPos.x = gnsstk::Triple(-16208820.579, -207275.833, 21038422.516);
      // The rest are just bunk with the intent that if the algorithm
      // is changed to include the data somehow, the assertion fails.
   svPos.v = gnsstk::Triple(123,456,789);
   svPos.clkbias = 234;
   svPos.clkdrift = 345;
   svPos.relcorr = 456;
   gnsstk::CommonTime when(gnsstk::CivilTime(2006,10,1,4,30,0,
                                             gnsstk::TimeSystem::Any));
   gnsstk::SatID sat(27, gnsstk::SatelliteSystem::GPS);
   gnsstk::ObsID oid(gnsstk::ObservationType::Phase, gnsstk::CarrierBand::L1,
                     gnsstk::TrackingCode::CA);
   gnsstk::NavType nav = gnsstk::NavType::GPSLNAV;
   ndf = std::make_shared<gnsstk::MultiFormatNavDataFactory>();
   gnsstk::MultiFormatNavDataFactory *mfndf =
      dynamic_cast<gnsstk::MultiFormatNavDataFactory*>(ndf.get());
   TUASSERTE(bool, true, mfndf->addDataSource(dataPath + "mixed.06n"));
   TUCATCH(navLib.addFactory(ndf));
   double corr = 0.0;
   TUASSERTE(bool, true, uut.getCorr(stnPos, svPos, sat, oid, when, nav, corr));
   TUASSERTFE(3.623343027333741, corr);
   TURETURN();
}


int main()
{
   unsigned errorTotal = 0;
   BCIonoCorrector_T testClass;

   errorTotal += testClass.constructorTest();
   errorTotal += testClass.getCorrTestPosition();
   errorTotal += testClass.getCorrTestXvt();

   std::cout << "Total Failures for " << __FILE__ << ": " << errorTotal
             << std::endl;

   return errorTotal;
}
