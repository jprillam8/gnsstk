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

#include "YDSTime.hpp"
#include "NBTropModel.hpp"

#define THROW_IF_INVALID_DETAILED() {if (!valid) {                   \
         InvalidTropModel e;                                            \
         if(!validWeather) e.addText("Invalid trop model: weather");    \
         if(!validRxLatitude)  e.addText("Invalid trop model: validRxLatitude"); \
         if(!validRxHeight)   e.addText("Invalid trop model: validRxHeight"); \
         if(!validDOY)   e.addText("Invalid trop model: day of year");  \
         GNSSTK_THROW(e);}}

namespace gnsstk
{
   static const double NBRd=287.054;   // J/kg/K = m*m*K/s*s
   static const double NBg=9.80665;    // m/s*s
   static const double NBk1=77.604;    // K/mbar
   static const double NBk3p=382000;   // K*K/mbar

   static const double NBLat[5]={   15.0,   30.0,   45.0,   60.0,   75.0};

   // zenith delays, averages
   static const double NBZP0[5]={1013.25,1017.25,1015.75,1011.75,1013.00};
   static const double NBZT0[5]={ 299.65, 294.15, 283.15, 272.15, 263.65};
   static const double NBZW0[5]={  26.31,  21.79,  11.66,   6.78,   4.11};
   static const double NBZB0[5]={6.30e-3,6.05e-3,5.58e-3,5.39e-3,4.53e-3};
   static const double NBZL0[5]={   2.77,   3.15,   2.57,   1.81,   1.55};

   // zenith delays, amplitudes
   static const double NBZPa[5]={    0.0,  -3.75,  -2.25,  -1.75,  -0.50};
   static const double NBZTa[5]={    0.0,    7.0,   11.0,   15.0,   14.5};
   static const double NBZWa[5]={    0.0,   8.85,   7.24,   5.36,   3.39};
   static const double NBZBa[5]={    0.0,0.25e-3,0.32e-3,0.81e-3,0.62e-3};
   static const double NBZLa[5]={    0.0,   0.33,   0.46,   0.74,   0.30};

   // mapping function, dry, averages
   static const double NBMad[5]={1.2769934e-3,1.2683230e-3,1.2465397e-3,1.2196049e-3,
                                 1.2045996e-3};
   static const double NBMbd[5]={2.9153695e-3,2.9152299e-3,2.9288445e-3,2.9022565e-3,
                                 2.9024912e-3};
   static const double NBMcd[5]={62.610505e-3,62.837393e-3,63.721774e-3,63.824265e-3,
                                 64.258455e-3};

   // mapping function, dry, amplitudes
   static const double NBMaa[5]={0.0,1.2709626e-5,2.6523662e-5,3.4000452e-5,
                                 4.1202191e-5};
   static const double NBMba[5]={0.0,2.1414979e-5,3.0160779e-5,7.2562722e-5,
                                 11.723375e-5};
   static const double NBMca[5]={0.0,9.0128400e-5,4.3497037e-5,84.795348e-5,
                                 170.37206e-5};

   // mapping function, wet, averages (there are no amplitudes for wet)
   static const double NBMaw[5]={5.8021897e-4,5.6794847e-4,5.8118019e-4,5.9727542e-4,
                           6.1641693e-4};
   static const double NBMbw[5]={1.4275268e-3,1.5138625e-3,1.4572752e-3,1.5007428e-3,
                           1.7599082e-3};
   static const double NBMcw[5]={4.3472961e-2,4.6729510e-2,4.3908931e-2,4.4626982e-2,
                           5.4736038e-2};

   // labels for use with the interpolation routine
   enum TableEntry { ZP=1, ZT, ZW, ZB, ZL, Mad, Mbd, Mcd, Maw, Mbw, Mcw };

   // the interpolation routine
   static double NB_Interpolate(double lat, int doy, TableEntry entry)
   {
      const double *pave = NULL, *pamp = NULL;
      double ret, day=double(doy);

         // assign pointer to the right array
      switch(entry) {
         case ZP:  pave=&NBZP0[0]; pamp=&NBZPa[0]; break;
         case ZT:  pave=&NBZT0[0]; pamp=&NBZTa[0]; break;
         case ZW:  pave=&NBZW0[0]; pamp=&NBZWa[0]; break;
         case ZB:  pave=&NBZB0[0]; pamp=&NBZBa[0]; break;
         case ZL:  pave=&NBZL0[0]; pamp=&NBZLa[0]; break;
         case Mad: pave=&NBMad[0]; pamp=&NBMaa[0]; break;
         case Mbd: pave=&NBMbd[0]; pamp=&NBMba[0]; break;
         case Mcd: pave=&NBMcd[0]; pamp=&NBMca[0]; break;
         case Maw: pave=&NBMaw[0];                 break;
         case Mbw: pave=&NBMbw[0];                 break;
         case Mcw: pave=&NBMcw[0];                 break;
         default:
         {
            InvalidRequest exc("NB_Interpolate called with unknown entry");
            GNSSTK_THROW(exc);
         }
      }

         // find the index i such that NBLat[i] <= lat < NBLat[i+1]
      int i = int(ABS(lat)/15.0)-1;

      if(i>=0 && i<=3) {               // mid-latitude -> regular interpolation
         double m=(ABS(lat)-NBLat[i])/(NBLat[i+1]-NBLat[i]);
         ret = pave[i]+m*(pave[i+1]-pave[i]);
         if(entry < Maw)
            ret -= (pamp[i]+m*(pamp[i+1]-pamp[i]))
               * std::cos(TWO_PI*(day-28.0)/365.25);
      }
      else {                           // < 15 or > 75 -> simpler interpolation
         if(i<0) i=0; else i=4;
         ret = pave[i];
         if(entry < Maw)
            ret -= pamp[i]*std::cos(TWO_PI*(day-28.0)/365.25);
      }

      return ret;

   }  // end double NB_Interpolate(lat,doy,entry)


   NBTropModel::NBTropModel():
      validWeather(false), validRxLatitude(false),
      validDOY(false), validRxHeight(false)
   {}


   NBTropModel::NBTropModel(const double& lat,
                            const int& day)
         : validWeather(false), validRxLatitude(false), validDOY(false),
           validRxHeight(false)
   {
      setReceiverLatitude(lat);
      setDayOfYear(day);
      setWeather();
   }


   NBTropModel::NBTropModel(const double& lat,
                            const int& day,
                            const WxObservation& wx)
         : validWeather(false), validRxLatitude(false), validDOY(false),
           validRxHeight(false)
   {
      setReceiverLatitude(lat);
      setDayOfYear(day);
      setWeather(wx);
   }


   NBTropModel::NBTropModel(const double& lat,
                            const int& day,
                            const double& T,
                            const double& P,
                            const double& H)
         : validWeather(false), validRxLatitude(false), validDOY(false),
           validRxHeight(false)
   {
      setReceiverLatitude(lat);
      setDayOfYear(day);
      setWeather(T,P,H);
   }


   NBTropModel::NBTropModel(const double& ht,
                            const double& lat,
                            const int& day)
         : validWeather(false), validRxLatitude(false), validDOY(false),
           validRxHeight(false)
   {
      setReceiverHeight(ht);
      setReceiverLatitude(lat);
      setDayOfYear(day);
      setWeather();
   }


      // re-define this to get the throws
   double NBTropModel::correction(double elevation) const
   {
      THROW_IF_INVALID_DETAILED();

      if(elevation < 0.0) return 0.0;

      return (dry_zenith_delay() * dry_mapping_function(elevation)
            + wet_zenith_delay() * wet_mapping_function(elevation));
   }


   double NBTropModel::correction(const Position& RX,
                                  const Position& SV,
                                  const CommonTime& tt)
   {
      THROW_IF_INVALID_DETAILED();

         // compute height and latitude from RX
      setReceiverHeight(RX.getHeight());
      setReceiverLatitude(RX.getGeodeticLatitude());

         // compute day of year from tt
      setDayOfYear(int((static_cast<YDSTime>(tt)).doy));

      return TropModel::correction(RX.elevation(SV));
   }


   double NBTropModel::correction(const Xvt& RX,
                                  const Xvt& SV,
                                  const CommonTime& tt)
   {
      Position R(RX),S(SV);
      return NBTropModel::correction(R,S,tt);
   }


   double NBTropModel::dry_zenith_delay() const
   {
      THROW_IF_INVALID_DETAILED();

      double beta = NB_Interpolate(latitude,doy,ZB);
      double gm = 9.784*(1.0-2.66e-3*std::cos(2.0*latitude*DEG_TO_RAD)-2.8e-7*height);

         /* scale factors for height above mean sea level
          * if weather is given, assume it's measured at ht -> kw=kd=1
          */
      double kd=1, base=std::log(1.0-beta*height/temp);
      if(interpolateWeather)
         kd = std::exp(base*NBg/(NBRd*beta));

         // compute the zenith delay for dry component
      return ((1.0e-6*NBk1*NBRd/gm) * kd * press);

   }  // end NBTropModel::dry_zenith_delay()


   double NBTropModel::wet_zenith_delay() const
   {
      THROW_IF_INVALID_DETAILED();

      double beta = NB_Interpolate(latitude,doy,ZB);
      double lam = NB_Interpolate(latitude,doy,ZL);
      double gm = 9.784*(1.0-2.66e-3*std::cos(2.0*latitude*DEG_TO_RAD)-2.8e-7*height);

         /* scale factors for height above mean sea level
          * if weather is given, assume it's measured at ht -> kw=kd=1
          */
      double kw=1, base=std::log(1.0-beta*height/temp);
      if(interpolateWeather)
         kw = std::exp(base*(-1.0+(lam+1)*NBg/(NBRd*beta)));

         // compute the zenith delay for wet component
      return ((1.0e-6*NBk3p*NBRd/(gm*(lam+1)-beta*NBRd)) * kw * humid/temp);

   }  // end NBTropModel::wet_zenith_delay()


   double NBTropModel::dry_mapping_function(double elevation) const
   {
      THROW_IF_INVALID_DETAILED();

      if(elevation < 0.0) return 0.0;

      double a,b,c,se,map;
      se = std::sin(elevation*DEG_TO_RAD);

      a = NB_Interpolate(latitude,doy,Mad);
      b = NB_Interpolate(latitude,doy,Mbd);
      c = NB_Interpolate(latitude,doy,Mcd);
      map = (1.0+a/(1.0+b/(1.0+c))) / (se+a/(se+b/(se+c)));

      a = 2.53e-5;
      b = 5.49e-3;
      c = 1.14e-3;
      if(ABS(elevation)<=0.001) se=0.001;
      map += ((1.0/se)-(1.0+a/(1.0+b/(1.0+c)))/(se+a/(se+b/(se+c))))*height/1000.0;

      return map;

   }  // end NBTropModel::dry_mapping_function()


   double NBTropModel::wet_mapping_function(double elevation) const
   {
      THROW_IF_INVALID_DETAILED();

      if(elevation < 0.0) return 0.0;

      double a,b,c,se;
      se = std::sin(elevation*DEG_TO_RAD);
      a = NB_Interpolate(latitude,doy,Maw);
      b = NB_Interpolate(latitude,doy,Mbw);
      c = NB_Interpolate(latitude,doy,Mcw);

      return ( (1.0+a/(1.0+b/(1.0+c))) / (se+a/(se+b/(se+c))) );

   }  // end NBTropModel::wet_mapping_function()


   void NBTropModel::setWeather(const double& T,
                                const double& P,
                                const double& H)
   {
      interpolateWeather=false;
      TropModel::setWeather(T,P,H);
            // humid actually stores water vapor partial pressure
      double th=300./temp;
      humid = 2.409e9*H*th*th*th*th*std::exp(-22.64*th);
      validWeather = true;
      valid = validWeather && validRxHeight && validRxLatitude && validDOY;

   }  // end NBTropModel::setWeather()


   void NBTropModel::setWeather(const WxObservation& wx)
   {
      interpolateWeather = false;
      try
      {
         TropModel::setWeather(wx);
            // humid actually stores vapor partial pressure
         double th=300./temp;
         humid = 2.409e9*humid*th*th*th*th*std::exp(-22.64*th);
         validWeather = true;
         valid = validWeather && validRxHeight && validRxLatitude && validDOY;
      }
      catch(InvalidParameter& e)
      {
         valid = validWeather = false;
         GNSSTK_RETHROW(e);
      }
   }


   void NBTropModel::setWeather()
   {
      interpolateWeather = true;

      if(!validRxLatitude)
      {
         valid = validWeather = false;
         InvalidTropModel e("NBTropModel must have Rx latitude before interpolating weather");
         GNSSTK_THROW(e);
      }
      if(!validDOY)
      {
         valid = validWeather = false;
         InvalidTropModel e("NBTropModel must have day of year before interpolating weather ");
         GNSSTK_THROW(e);
      }
      temp = NB_Interpolate(latitude,doy,ZT);
      press = NB_Interpolate(latitude,doy,ZP);
      humid = NB_Interpolate(latitude,doy,ZW);
      validWeather = true;
      valid = validWeather && validRxHeight && validRxLatitude && validDOY;
   }


   void NBTropModel::setReceiverHeight(const double& ht)
   {
      height = ht;
      validRxHeight = true;
      valid = validWeather && validRxHeight && validRxLatitude && validDOY;
      if(!validWeather && validRxLatitude && validDOY)
         setWeather();
   }


   void NBTropModel::setReceiverLatitude(const double& lat)
   {
      latitude = lat;
      validRxLatitude = true;
      valid = validWeather && validRxHeight && validRxLatitude && validDOY;
      if(!validWeather && validRxLatitude && validDOY)
         setWeather();
   }


   void NBTropModel::setDayOfYear(const int& d)
   {
      doy = d;
      if (doy > 0 && doy < 367) validDOY=true; else validDOY = false;
      valid = validWeather && validRxHeight && validRxLatitude && validDOY;
      if(!validWeather && validRxLatitude && validDOY)
         setWeather();
   }

}
