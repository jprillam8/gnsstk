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
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
//  
//  Copyright 2008, The University of Texas at Austin
//
//============================================================================
//
// Compute number of stations visible to a set of space vehicles (SV) 
// over a requested period (default 23:56).  Accept FIC 
// almanac format, FIC ephemeris, Rinex nav, Yuma almanac, SEM almanac, 
// or SP3 as input
//
// Assumptions:
//
// System
#include <stdio.h>
#include <iostream>
#include <string>
#include <list>

// gpstk
#include "BasicFramework.hpp"
#include "StringUtils.hpp"
#include "TimeString.hpp"
#include "CommandOptionWithTimeArg.hpp"
#include "CommonTime.hpp"
#include "GPSWeekSecond.hpp"
#include "TimeConstants.hpp"
#include "YDSTime.hpp"
#include "SystemTime.hpp"
#include "AlmOrbit.hpp"
#include "GPSAlmanacStore.hpp"
#include "YumaAlmanacStore.hpp"
#include "SEMAlmanacStore.hpp"
#include "GPSEphemerisStore.hpp"
//#include "icd_gps_constants.hpp"
#include "gps_constants.hpp"
#include "CivilTime.hpp"
#include "YDSTime.hpp"

// Project
#include "VisSupport.hpp"
#include "StaStats.hpp"
#include "DiscreteVisibleCounts.hpp"

using namespace std;
using namespace gpstk;

class compSatVis : public gpstk::BasicFramework
{
public:
   compSatVis(const std::string& applName,
              const std::string& applDesc) throw();
   ~compSatVis() {}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
   virtual bool initialize(int argc, char *argv[]) throw();
#pragma clang diagnostic pop
protected:
   virtual void process();

   gpstk::CommandOptionWithAnyArg intervalOpt;
   gpstk::CommandOptionWithAnyArg outputOpt;
   gpstk::CommandOptionWithAnyArg nFileNameOpt;
   gpstk::CommandOptionWithAnyArg mscFileName;
   gpstk::CommandOptionWithAnyArg minElvOpt;
   gpstk::CommandOptionWithAnyArg typeOpt;
   gpstk::CommandOptionWithAnyArg minStaOpt;
   gpstk::CommandOptionNoArg detailPrintOpt;
   gpstk::CommandOptionWithAnyArg excludeStation;
   gpstk::CommandOptionWithAnyArg includeStation;
   gpstk::CommandOptionWithTimeArg evalStartTimeOpt;
   gpstk::CommandOptionWithTimeArg evalEndTimeOpt;
   std::list<long> blocklist;
   
   FILE *logfp;

   static const int FIC_ALM;
   static const int FIC_EPH;
   static const int RINEX_NAV;
   static const int Yuma_ALM;
   static const int SEM_ALM;
   static const int SP3;
   int navFileType; 
   
   bool detailPrint;
   bool evalStartTimeSet;
   CommonTime evalStartTime;
   bool evalEndTimeSet;
   CommonTime evalEndTime;
   
   double intervalInSeconds;
   double minimumElevationAngle;
   int minStationCount;
   
   GPSAlmanacStore BCAlmList;
   GPSEphemerisStore BCEphList;
   SP3EphemerisStore SP3EphList;
   YumaAlmanacStore YumaAlmStore;
   SEMAlmanacStore SEMAlmStore;

   StaPosList stationPositions;
      
      // Storage for min, max , avg. statistics.  Storage is both by-SV 
      // and over the entire constellation
   typedef map<int,StaStats> StaStatsList;
   StaStatsList staStatsList;
   StaStats statsOverAllPRNs;
   int epochCount;
   
     // Storage for the literal count of number of epochs with a particular
     // number of stations in view.  It turns out we frequently care not
     // only about min/max/avg but the fraction of time a typical SV is in
     // view of "at least X" number of stations.  This is partially addressed
     // by the -m option which allows the user to specify a particular number
     // of interest, but we frequently want to know this information for the 
     // full range of stations.
   map <int,DiscreteVisibleCounts> dvcList;
   
   CommonTime startT;
   CommonTime endT;
   bool siderealDay; 
   
   void generateHeader( gpstk::CommonTime currT );
   void generateTrailer( );
   gpstk::CommonTime setStartTime();
   void computeVisibility( gpstk::CommonTime currT );
   void printNavFileReferenceTime(FILE* logfp);
};

const int compSatVis::FIC_ALM = 0;
const int compSatVis::FIC_EPH = 1;
const int compSatVis::RINEX_NAV = 2;
const int compSatVis::SP3 = 3;
const int compSatVis::Yuma_ALM = 4;
const int compSatVis::SEM_ALM = 5;

int main( int argc, char*argv[] )
{
   try
   {
      compSatVis fc("compSatVis", "Compute Satellite Visiblity.");
      if (!fc.initialize(argc, argv)) return(false);
      fc.run();
   }
   catch(gpstk::Exception& exc)
   {
      cout << exc << endl;
      return 1;
   }
   catch(...)
   {
      cout << "Caught an unnamed exception. Exiting." << endl;
      return 1;
   }
   return 0;
}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreorder"
compSatVis::compSatVis(const std::string& applName, 
                       const std::string& applDesc) throw()
          :BasicFramework(applName, applDesc),
           statsOverAllPRNs("0",0,0),
           intervalOpt('p',"int","Interval in seconds.", false),
           nFileNameOpt('n',"nav","Name of navigation file" , true),
           outputOpt('o', "output-file", "Name of the output file to write.", true),
           mscFileName('c',"mscfile","Name of MS coordinates files", true),
           minElvOpt('e', "minelv","Minimum elevation angle.", false),
           excludeStation('x',"exclude","Exclude station",false),
           includeStation('i',"include","Include station",false),
           detailPrintOpt('D',"detail","Print SV count for each interval",false),
           minStaOpt('m',"min-sta","Minimum # of stations visible simultaneously",false),
           evalStartTimeOpt('s',"start-time","%m/%d/%y %H:%M","Start time of evaluation (\"m/d/y H:M\") ",false),
           evalEndTimeOpt('z',"end-time","%m/%d/%y %H:%M","End time of evaluation (\"m/d/y H:M\")",false),
           typeOpt('t', "navFileType", "FALM, FEPH, RNAV, YUMA, SEM, or SP3 ", false)
{
   intervalOpt.setMaxCount(1);
   nFileNameOpt.setMaxCount(3);        // 3 to allow for SP3 input
   outputOpt.setMaxCount(1);
   minElvOpt.setMaxCount(1);
   mscFileName.setMaxCount(1);
   typeOpt.setMaxCount(1);
   detailPrintOpt.setMaxCount(1);
   evalStartTimeOpt.setMaxCount(1);
   evalStartTimeSet = false;
   evalEndTimeOpt.setMaxCount(1);
   evalEndTimeSet = false;
   
   epochCount = 0;
}
#pragma clang diagnostic pop
bool compSatVis::initialize(int argc, char *argv[])
   throw()
{
   if (!BasicFramework::initialize(argc, argv)) return false;
   
      // Open the output file
   logfp = fopen( outputOpt.getValue().front().c_str(),"wt");
   if (logfp==0) 
   {
      cout << "Failed to open output file. Exiting." << endl;
      return false;
   }
   
      // Set up starting CommonTime, ending CommonTime, number and size of intervals
   vector<string> values;
   intervalInSeconds = 60.0;        // default
   if (intervalOpt.getCount()>0)
   {
      values = intervalOpt.getValue();
      intervalInSeconds = StringUtils::asInt( values[0] );
   }

   navFileType = FIC_ALM;              // default case
   if (typeOpt.getCount()!=0)
   {
      values = typeOpt.getValue();
      if (values[0].compare("FEPH")==0)      navFileType = FIC_EPH;
      else if (values[0].compare("RNAV")==0) navFileType = RINEX_NAV;
      else if (values[0].compare("SP3")==0)  navFileType = SP3;
      else if (values[0].compare("FALM")==0) navFileType = FIC_ALM;
      else if (values[0].compare("YUMA")==0) navFileType = Yuma_ALM;
      else if (values[0].compare("SEM")==0)  navFileType = SEM_ALM;
      else
      {
         cerr << "Invalid value for nav file type.  Must be one of " << endl;
         cerr << "   'FALM', 'FEPH', 'RNAV', or 'SP3'. " << endl;
         cerr << "Fatal error.  compSatVis will terminate." << endl;
         return false;
      }
   }

   minimumElevationAngle = 10.0;
   if (minElvOpt.getCount()!=0)
   {
      values = minElvOpt.getValue();
      minimumElevationAngle = StringUtils::asDouble( values[0] ); 
   }
   
   detailPrint = false;
   if (detailPrintOpt.getCount()!=0) detailPrint = true;

   minStationCount = 2;
   if (minStaOpt.getCount()!=0)
   {
      values = minStaOpt.getValue();
      minStationCount = StringUtils::asInt( values[0] );
   }

      // Initialize the statistics objects   
   for (int PRNID=1;PRNID<=gpstk::MAX_PRN;++PRNID)
   {
      StaStats temp = StaStats( StringUtils::asString(PRNID), 0, minStationCount );
      pair<int,StaStats> node( PRNID, temp );
      staStatsList.insert( node );
       
      DiscreteVisibleCounts dvcTemp = DiscreteVisibleCounts();
      pair<int,DiscreteVisibleCounts> dvcNode(PRNID, dvcTemp);
      dvcList.insert(dvcNode);
   }
   statsOverAllPRNs.updateMinStations( minStationCount );
   DiscreteVisibleCounts dvcTemp = DiscreteVisibleCounts();
   pair<int,DiscreteVisibleCounts> dvcNode(0, dvcTemp);
   dvcList.insert(dvcNode);
  
      // If the user SPECIFIED a start time for the evaluation, store that
      // time and set the flag.
   evalStartTimeSet = false;
   evalStartTime=CommonTime::BEGINNING_OF_TIME;
   if (evalStartTimeOpt.getCount()!=0) 
   {
      if (debugLevel) cout << "Reading start time from command line." << endl;
      std::vector<CommonTime> tvalues = evalStartTimeOpt.getTime();
      evalStartTime = tvalues[0];
      evalStartTimeSet = true;
      
         // Reinit YumaAlmStore to know the time of interest..
      if (navFileType==Yuma_ALM) YumaAlmStore = YumaAlmanacStore( evalStartTime );
      if (navFileType==SEM_ALM) SEMAlmStore = SEMAlmanacStore( evalStartTime );
   }
   
      // If the user SPECIFIED an end time for the evaluation, store that
      // time and set the flag.
   evalEndTimeSet = false;
   evalEndTime=CommonTime::END_OF_TIME;
   if (evalEndTimeOpt.getCount()!=0) 
   {
      if (debugLevel) cout << "Reading end time from command line." << endl;
      std::vector<CommonTime> tvalues = evalEndTimeOpt.getTime();
      evalEndTime = tvalues[0];
      evalEndTimeSet = true;
   }
   
   return true;   
}

void compSatVis::printNavFileReferenceTime(FILE* logfp)
{
   string tform2 = "%02m/%02d/%02y DOY %03j, GPS Week %F, DOW %w, %02H:%02M:%02S";
   CommonTime t;
   
      // If the user did not specify a start time for the evaulation, find the
      // epoch time of the navigation data set and work from that.
      // In the case of almanac data, the "initial time" is derived from the 
      // earliest almanac reference time minus a half week.  Therefore, we
      // add the halfweek back in.  
   switch(navFileType)
   {
         // For ephemeris, initial time is earliest beginning of effectivty.
      case FIC_EPH:
      case RINEX_NAV:
	 t = BCEphList.getInitialTime();
	 t += HALFWEEK;
         fprintf(logfp,"  Ephemeris effectivity\n");
         fprintf(logfp,"     Earliest             : %s\n",
                 printTime(t,tform2).c_str());
	 t = BCEphList.getFinalTime();
	 t -= HALFWEEK;
         fprintf(logfp,"     Latest               : %s\n",
                 printTime(t,tform2).c_str());
         break;
            
      case FIC_ALM:
         t = BCAlmList.getInitialTime();
         t += HALFWEEK;
         fprintf(logfp,"  Almanac reference time\n");
         fprintf(logfp,"     Earliest             : %s\n",
                       printTime(t,tform2).c_str());
         t = BCAlmList.getFinalTime();
         t -= HALFWEEK;
         fprintf(logfp,"     Latest               : %s\n",
                       printTime(t,tform2).c_str());
         break;

      case Yuma_ALM:
         t = YumaAlmStore.getInitialTime();
         t += HALFWEEK;
         fprintf(logfp,"  Almanac reference time\n");
         fprintf(logfp,"     Earliest             : %s\n",
                       printTime(t,tform2).c_str());
         t = YumaAlmStore.getFinalTime();
         t -= HALFWEEK;
         fprintf(logfp,"     Latest               : %s\n",
                       printTime(t,tform2).c_str());
         break;

      case SEM_ALM:
         t = SEMAlmStore.getInitialTime();
         t += HALFWEEK;
         fprintf(logfp,"  Almanac reference time\n");
         fprintf(logfp,"     Earliest             : %s\n",
                       printTime(t,tform2).c_str());
         t = SEMAlmStore.getFinalTime();
         t -= HALFWEEK;
         fprintf(logfp,"     Latest               : %s\n",
                       printTime(t,tform2).c_str());
         break;
         
      case SP3:
      {
         CommonTime begin = SP3EphList.getInitialTime();
         CommonTime end = SP3EphList.getFinalTime();
         fprintf(logfp,"  Ephemeris effectivity\n");
         fprintf(logfp,"     Earliest             : %s\n",
                 printTime(begin,tform2).c_str());
         fprintf(logfp,"     Latest               : %s\n",
                 printTime(end,tform2).c_str());
         break;
      }   
   }
   return;
}

gpstk::CommonTime compSatVis::setStartTime()
{
   CommonTime retDT = GPSWeekSecond( 621, 0.0 );     // 12/1/1991
   CommonTime initialTime;
   CommonTime finalTime;
   
   switch(navFileType)
   {
      case FIC_EPH:
      case RINEX_NAV:
         initialTime = BCEphList.getInitialTime();
         finalTime   = BCEphList.getFinalTime();
         break;
            
      case FIC_ALM:
         initialTime = BCAlmList.getInitialTime();
         finalTime   = BCAlmList.getFinalTime();
         break;

      case Yuma_ALM:
         initialTime = YumaAlmStore.getInitialTime();
         finalTime   = YumaAlmStore.getFinalTime();
         break;

      case SEM_ALM:
         initialTime = SEMAlmStore.getInitialTime();
         finalTime   = SEMAlmStore.getFinalTime();
         break;

         // If loading "day at a time" files, will need 
         // three days to cover middle day.  We need to 
         // find the middle of whatever period was loaded
         // and back up to the beginning of that day.
      case SP3:
      {
         initialTime = SP3EphList.getInitialTime();
         finalTime = SP3EphList.getFinalTime();
         break;
      }   
   }
   double diff = finalTime - initialTime;
   retDT = initialTime;
   retDT += diff/2.0;
   retDT = YDSTime( static_cast<YDSTime>(retDT).year, static_cast<YDSTime>(retDT).doy, 0.0 );
   return(retDT);
}        

void compSatVis::process()
{
      // Read navigation file input
   if (verboseOption)
   {
      cout << "Loading navigation message data from ";
      int nfiles = nFileNameOpt.getCount();
      vector<std::string> names = nFileNameOpt.getValue();
      for (int i1=0;i1<nfiles;++i1) 
      {
         if (i1>0) cout << ", ";
         cout << names[i1];
      }
      cout << "." << endl;
   }
   switch(navFileType)
   {
      case FIC_EPH: VisSupport::readFICNavData(nFileNameOpt,BCAlmList,BCEphList); break;
      case FIC_ALM: VisSupport::readFICNavData(nFileNameOpt,BCAlmList,BCEphList); break;
      case RINEX_NAV: VisSupport::readRINEXNavData(nFileNameOpt,BCEphList); break;
      case Yuma_ALM: VisSupport::readYumaData(nFileNameOpt,YumaAlmStore); break; 
      case SEM_ALM:  VisSupport::readSEMData(nFileNameOpt,SEMAlmStore); break;
      case SP3: VisSupport::readPEData(nFileNameOpt,SP3EphList); break;
      default:
         cerr << "Unknown navigation file type in process()." << endl;
         cerr << "Fatal error. compSatVis will halt." << endl; exit(1);
   }
   
      // Determine day of interest
   if (debugLevel) cout << "Setting evaluation start time: ";
   startT = evalStartTime;
   if (!evalStartTimeSet) startT = setStartTime();
   if (debugLevel) cout << printTime(startT, "%02m/%02d/%02y DOY %03j, GPS Week %F, DOW %w, %02H:%02M.") << endl;
   
      // If no end time commanded, compute for 23h 56m (GPS ground track repeat)
   if (debugLevel) cout << "Setting evaluation end time: ";
   siderealDay = true;
   endT = startT + ( (double) SEC_PER_DAY - 240.0);
   if (evalEndTimeSet) endT = evalEndTime;
   if ((int)(endT-startT)!=(int)(SEC_PER_DAY-240)) siderealDay = false;
   if (debugLevel) 
   {
      cout << printTime(endT,"%02m/%02d/%02y DOY %03j, GPS Week %F, DOW %w, %02H:%02M.") << endl;
      cout << "Sidereal Day flag : " << siderealDay << endl;
   }
   CommonTime currT = startT;   
   
      // Get coordinates for the stations
   stationPositions = VisSupport::getStationCoordinates( mscFileName,
                                                         startT, 
                                                         includeStation, 
                                                         excludeStation );
   
      // Generate the header
   generateHeader( startT );
      
      // For each interval, calculate SV-station visibility
   if (debugLevel) cout << "Entering calculation loop." << endl;
   long lastValue = -1;
   while (currT <= endT) 
   {
      if (debugLevel)
      {
         long sec = (long) static_cast<GPSWeekSecond>(currT).sow;
         long newValue = sec / 3600;
         if (newValue!=lastValue)
         {
            if (static_cast<CivilTime>(currT).hour==0) cout << printTime(currT,"%02m/%02d/%04Y ");
            cout << printTime(currT,"%02H:, ");
            lastValue = newValue;
         }
      }
      computeVisibility( currT );
      currT += intervalInSeconds;
      epochCount++;
   }
   
   if (debugLevel) cout << endl << "Generating trailer." << endl;
   generateTrailer( );
  
   fclose(logfp);
   
}

void compSatVis::generateHeader( gpstk::CommonTime currT )
{
   CommonTime sysTime = SystemTime();
   string tform = "%02m/%02d/%02y DOY %03j, GPS Week %F, DOW %w";
   fprintf(logfp,"compSatVis output file.  Generated at %s\n",
           printTime(sysTime,"%02H:%02M on %02m/%02d/%02y").c_str() );
   fprintf(logfp,"Program arguments:\n");
   fprintf(logfp,"  Navigation file         : ");
   vector<std::string> values = nFileNameOpt.getValue();
   for (size_t i=0; i<nFileNameOpt.getCount(); ++i) 
      fprintf(logfp,"%s  ",values[i].c_str());
   fprintf(logfp,"\n");
   fprintf(logfp,"  Day of interest         : %s\n",printTime(currT,tform).c_str());
   fprintf(logfp,"  Minimum elv ang         : %5.0f degrees\n",minimumElevationAngle);
   fprintf(logfp,"  Evaluation interval     : %5.0f sec\n",intervalInSeconds);
   fprintf(logfp,"  Station coordinates file: %s\n",mscFileName.getValue().front().c_str());
   printNavFileReferenceTime(logfp);
   fprintf(logfp,"  Start time of evaluation: %s\n",printTime(startT,tform+", %02H:%02M:%02S").c_str());
   fprintf(logfp,"  End time of evaluation  : %s\n",printTime(endT,tform+", %02H:%02M:%02S").c_str());
   if (siderealDay)
      fprintf(logfp,"  Evaluation covers one sidereal day.\n");
   
      // Print list of stations
   if (includeStation.getCount() || excludeStation.getCount() )
   {
      fprintf(logfp,"\n  Stations included in the analysis\n");
      fprintf(logfp," Abbr       XYZ(km)\n");
      StaPosList::const_iterator si;
      for (si=stationPositions.begin();si!=stationPositions.end();++si)
      {
         string mnemonic = (string) si->first;
         Position coordinates(si->second);
         fprintf(logfp," %4s  %10.3lf  %10.3lf  %10.3lf\n",
              mnemonic.c_str(),
              coordinates[0]/1000.0,
              coordinates[1]/1000.0,
              coordinates[2]/1000.0 );
      }
      int nSta = stationPositions.size();
      fprintf(logfp,"Number of Stations: %d\n\n", nSta);
   }
   else fprintf(logfp,"  All stations in coordinates file were included in the analysis.");
}

void compSatVis::generateTrailer( )
{
   fprintf(logfp,"\n\n Summary statistics by satellite\n");
   fprintf(logfp," All values based on evaluation of a single sidereal day\n\n");
   fprintf(logfp,"   SV     Avg  !        Minimum        !      Maximum          !  #Mins\n");
   fprintf(logfp," PRN ID  #Stas ! #Sta Dur(min)  #Occur ! #Sta Dur(min)  #Occur ! < %d Stas\n",
           minStationCount);
   
   int dum2 = (int) intervalInSeconds;
   
   StaStatsList::const_iterator sslCI;
   for (sslCI =staStatsList.begin();
        sslCI!=staStatsList.end(); ++sslCI)
   {
      int staNum = sslCI->first;
#pragma unused(staNum)
      StaStats ss = sslCI->second;
      if (ss.atLeastOneEntry()) 
      {
         std::string dummy = ss.getSatStr( dum2 );
         fprintf(logfp,"%s\n",dummy.c_str());
      }
   }
   fprintf(logfp,"%s\n",statsOverAllPRNs.getSatAvgStr( ).c_str());
   
      // Now output the percentages related to the fraction of time
      // a particular number of stations see a SV and the average
      //
   const DiscreteVisibleCounts& dvc0 = dvcList.find(0)->second;
   int max = dvc0.getMaxCount();
   fprintf(logfp,"\n Fraction of time an SV is visible to a given number of stations.\n");
   fprintf(logfp,"PRN ID ");
   for (int i=0;i<=max;++i) fprintf(logfp,"    =%2d",i);
   fprintf(logfp,"\n");
   map<int,DiscreteVisibleCounts>::const_iterator CI;
   for (CI=dvcList.begin();CI!=dvcList.end();++CI)
   {
      int PRNID = CI->first;
      const DiscreteVisibleCounts& dvcR = (DiscreteVisibleCounts)CI->second;
      string str = dvcR.dumpCountsAsPercentages(max,7);
      if (PRNID==0)
         fprintf(logfp,"   Avg:%s\n",str.c_str());
       else
         fprintf(logfp,"    %02d:%s\n",PRNID,str.c_str());
   }
   fprintf(logfp,"\n");
   
   fprintf(logfp,"\n Fraction of time an SV is visible to >= a given number of stations.\n");
   fprintf(logfp,"PRN ID ");
   for (int i=0;i<=max;++i) fprintf(logfp,"   >=%2d",i);
   fprintf(logfp,"\n");
   for (CI=dvcList.begin();CI!=dvcList.end();++CI)
   {
      int PRNID = CI->first;
      const DiscreteVisibleCounts& dvcR = (DiscreteVisibleCounts)CI->second;
      string str = dvcR.dumpCumulativeCountsAsPercentages(max,7);
      if (PRNID==0)
         fprintf(logfp,"   Avg:%s\n",str.c_str());
       else
         fprintf(logfp,"    %02d:%s\n",PRNID,str.c_str());
   }
   fprintf(logfp,"\n");
}

void compSatVis::computeVisibility( gpstk::CommonTime currT )
{
   gpstk::Position SVpos[gpstk::MAX_PRN+1];
   bool SVAvail[gpstk::MAX_PRN+1];
   Xvt SVxvt;
   
      // Compute SV positions for this epoch
   int PRNID;
   for (PRNID=1;PRNID<=gpstk::MAX_PRN;++PRNID)
   {
      SVAvail[PRNID] = false;
      try
      {
         SatID satid(PRNID,SatID::systemGPS);
         switch(navFileType)
         {
            case FIC_EPH:
            case RINEX_NAV:
               SVxvt = BCEphList.getXvt( satid, currT );
               break;
            
            case FIC_ALM:
               SVxvt = BCAlmList.getXvt( satid, currT );
               break;
            
            case Yuma_ALM:
               SVxvt = YumaAlmStore.getXvt( satid, currT );
               break;
 
            case SEM_ALM:
               SVxvt = SEMAlmStore.getXvt( satid, currT );
               break;
            
            case SP3:
               SVxvt = SP3EphList.getXvt( satid, currT );
               break;
               
            default:
               cerr << "Unknown navigation file type in computeVisibility()." << endl;
               cerr << "Fatal error. compSatVis will halt." << endl;
               exit(1);
         }
         SVpos[PRNID] = SVxvt.x; 
         SVAvail[PRNID] = true;
      }
      catch(InvalidRequest& e)
      { 
         continue;
      }
   }
   
      // If first time through, print the detail hearder if required.
   if (detailPrint && currT==startT)
   {
      fprintf(logfp," Number of Stations visible to each SV by epoch\n");
      fprintf(logfp,"  PRN, ");
      for (int pi=1;pi<=gpstk::MAX_PRN;++pi)
      {
         if (SVAvail[pi]) fprintf(logfp," %4d,",pi);
      }
      fprintf(logfp,"   Max,   Min\n");
   }
   
   if (detailPrint) fprintf(logfp,"%s, ",printTime(currT,"%02H:%02M").c_str());

      // Now count number of Stations visible to each SV
   int maxNum = 0;
   int minNum = stationPositions.size() + 1; 
   StaPosList::const_iterator splCI;
   DiscreteVisibleCounts& dvc0 = dvcList.find(0)->second;
   
   for (PRNID=1;PRNID<=gpstk::MAX_PRN;++PRNID)
   {
      if (!SVAvail[PRNID]) continue;
      int numVis = 0;
      for (splCI =stationPositions.begin();
           splCI!=stationPositions.end();
           ++splCI)
      {
         Position staPos = splCI->second;
         double elv = staPos.elvAngle( SVpos[PRNID] );
         if (elv>=minimumElevationAngle) numVis++;
      }
      if (detailPrint) fprintf(logfp,"   %2d,",numVis);
      if (numVis>maxNum) maxNum = numVis;
      if (numVis<minNum) minNum = numVis;
      
      StaStatsList::iterator sslI = staStatsList.find( PRNID );
      if (sslI==staStatsList.end())
      {
         cerr << "Missing statistics object for satellite " << PRNID << endl;
         cerr << "Fatal error.  compSatVis will termination." << endl;
      }
      StaStats& ss = sslI->second;
      ss.addEpochInfo( numVis, epochCount );
      statsOverAllPRNs.addEpochInfo( numVis, epochCount );
      DiscreteVisibleCounts& dvc = dvcList.find(PRNID)->second;
      dvc.addCount(numVis);
      dvc0.addCount(numVis);
   }
   if (detailPrint) fprintf(logfp,"    %2d,    %2d\n",maxNum,minNum);
}
