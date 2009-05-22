#pragma ident "$Id$"

//============================================================================
//
//  This file is part of GPSTk, the GPS Toolkit.
//
//  The GPSTk is free software; you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation; either version 2.1 of the License, or
//  any later version.
//
//  The GPSTk is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with GPSTk; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  
//  Copyright 2004, The University of Texas at Austin
//
//============================================================================

/**
 * @file petest.cpp Read an SP3 format file into SP3EphemerisStore, edit and dump.
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <map>

#include "CommonTime.hpp"
#include "SP3Stream.hpp"
#include "SP3Data.hpp"
#include "SP3Header.hpp"
#include "SP3EphemerisStore.hpp"
#include "SatID.hpp"

using namespace std;
using namespace gpstk;

int main(int argc, char *argv[])
{
   if(argc<2) {
     cout << "Usage: petest <SP3-format files ...>" << endl;
     return -1;
   }

   try
   {
      bool firstEpochFound=true;
      CommonTime firstTime;
      CommonTime lastTime;
      SatID firstSat;

      int i,ip,it,nf=0,np=0,nt=0;
      SP3EphemerisStore EphList;

      // don't reject anything....
      EphList.rejectBadPositions(false);
      EphList.rejectBadClocks(false);

      // loop over file names on the command line
      for(i=1; i<argc; i++) {
         SP3Header header;
         SP3Data data;
         
         // you can't open, close, and reopen a file w/o abort on second open...
         SP3Stream pestrm;
         // SP3Stream does this ... pestrm.exceptions(ifstream::failbit);
         cout << "Reading SP3 file " << argv[i] << "." << endl;
         pestrm.open(argv[i],ios::in);

         pestrm >> header;
         
         header.dump(cout);

         ip = it = 0;
         CommonTime ttag(CommonTime::BEGINNING_OF_TIME);

         while(pestrm >> data) {
            data.dump(cout, header.version==SP3Header::SP3c);

            if(firstEpochFound && data.RecType == '*') {  
               firstTime = data.time;
               lastTime = firstTime;
            }
            else if(firstEpochFound) {
               firstSat = data.sat;
               firstEpochFound=false;
            }

            if(data.time > lastTime) lastTime = data.time;
            
            if(data.time > ttag) {
               ttag = data.time;
               it++; nt++;
            }
            ip++; np++;
         }
         cout << endl << "Done with file " << argv[i] << ": read "
              << ip << " P/V records and " << it << " epochs." << endl;
         for(int j=0; j<pestrm.warnings.size(); j++)
            cout << pestrm.warnings[j] << endl;

         pestrm.close();
         nf++;

         // add to store
         cout << endl << "Now load the file using SP3Ephemeris::loadFile()" << endl;
         EphList.loadFile(string(argv[i]));
      }
      
      cout << endl << "Done with " << nf << " files: read "
           << np << " P/V records and " << nt << " epochs." << endl;

      cout << "Interpolation order is " << EphList.getInterpolationOrder() << endl;
      cout << "Set order to 17" << endl;
      EphList.setInterpolationOrder(17);
      EphList.dump(cout, 2);
 
      const char *fmt="%4Y/%02m/%02d %2H:%02M:%02S (%P)";
      CommonTime ttf = firstTime + 3600., ttl = lastTime - 3600.;
      cout << endl << "Now edit the store to cut out the first and last hours: "
           << static_cast<CivilTime>(ttf).printf(fmt) << " to "
           << static_cast<CivilTime>(ttl).printf(fmt) << "." << endl;
      EphList.edit(ttf,ttl);
      EphList.dump(cout, 2);

      ttf = ttf + 3600.;
      cout << endl << "Now edit the store to cut out another hour at the first only: "
           << static_cast<CivilTime>(ttf).printf(fmt) << " to "
           << static_cast<CivilTime>(ttl).printf(fmt) << "." << endl;
      EphList.edit(ttf);
      EphList.dump(cout, 2);

      ttf = CommonTime::BEGINNING_OF_TIME;
      ttl = ttf + 14400.;
      cout << endl << "Now edit the store using bogus times: "
           << static_cast<CivilTime>(ttf).printf(fmt) << " to "
           << static_cast<CivilTime>(ttl).printf(fmt) << endl;
      EphList.edit(ttf,ttl);
      EphList.dump(cout, 2);
/*
      unsigned long ref;
      // choose a time tag within the data....
      CommonTime tt = firstTime + 3600.; // + (lastTime-firstTime)/2.;
      SatID tsat = firstSat;
      Xvt PVT;
      for(i=0; i<300; i++) {
         tt += 30.0;
         PVT = EphList.getXvt(tsat,tt);

         if(false) 
            cout << "LI " << tt << " P " << fixed
                 << setw(16) << setprecision(6) << PVT.x[0] << " "
                 << setw(16) << setprecision(6) << PVT.x[1] << " "
                 << setw(16) << setprecision(6) << PVT.x[2] << " "
                 << setw(10) << setprecision(6) << PVT.dtime
                 << " V "
                 << setw(12) << setprecision(6) << PVT.v[0] << " "
                 << setw(12) << setprecision(6) << PVT.v[1] << " "
                 << setw(12) << setprecision(6) << PVT.v[2] << " "
                 << setw(12) << setprecision(6) << PVT.ddtime
                 << endl;
      }
*/
   }
   catch(Exception& e) {
      cout << e.what();
      return -1;
   }
   catch(...) {
      cout << "Caught an unknown exception" << endl;
      return -1;
   }

   return 0;
}
