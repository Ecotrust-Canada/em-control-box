/*
EM: Electronic Monitor - Control Box Software
Copyright (C) 2012 Ecotrust Canada
Knowledge Systems and Planning

This file is part of EM.

EM is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

EM is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with EM. If not, see <http://www.gnu.org/licenses/>.

You may contact Ecotrust Canada via our website http://ecotrust.ca
*/

#include "geolib.h"
#include "../util.h"
#include <cstring>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>

using namespace std;

#define PI 3.14159265

int GPS_ERRORS = 0; ///< GPS error flags. 
int CounterForBlankOutput = 0; ///< If get several consecutive blank output from GPS, report an error.
timeval current_timeval;
time_t future_time_t;
bool get_valid_gps_time = false;

/**
 * This function checks the checksum for a sentence from the GPS sensor.
 * @param sentence The sentece to be checked.
 * @return true if checksum succeeded, false otherwise.
 *
 */
bool checkSumForSentence(char* sentence)
{
    if(strlen(sentence) < 2)
    {
        cerr << "WARN:GPS:Invalid sentence length" << endl;
        return false;
    }
    int tmp= sentence[1];
    char result[3];              //computed checksum.
    char sum[2];                 //check sum returned in the sentence.
    unsigned int i = 2;

    //According to the documentation, the checksum for the GPS sentence is specially checked,
    //i.e., compute xor of all the bits in the sentence.
    for(; i< strlen(sentence) && sentence[i] != '*'; i++)
    {
        tmp = tmp ^ sentence[i];
    }

    if(i == strlen(sentence))    //If checksum is missing, i.e., if we cannot find a '*' character in the sentence.
    {
        cerr << "WARN:GPS:Could not find checksum from the sentence" << endl;
        return false;
    }

    sprintf(result, "%x", tmp);  //convert to hex.
    sentence[i+ 3] = 0;
    if(sentence[i+1] == '0')
    {
        strcpy(sum, sentence+ i+ 2);
    }
    else
    {
        strcpy(sum, sentence+ i+ 1);
    }
    toUpperCase(result);
    if(strcmp(sum, result) == 0) //If two checksums match.
    {
        return true;
    }
    else
    {
        return false;
    }

}

/**
 * Reset all the GPS error flags.
 */
void ResetAllErrors()
{
    GPS_ERRORS = GPS_ERRORS & (~ GEO_NO_SAT_IN_USE);
    GPS_ERRORS = GPS_ERRORS & (~ GEO_NO_CONNECTION);
    GPS_ERRORS = GPS_ERRORS & (~ GEO_ESTIMATED);
    GPS_ERRORS = GPS_ERRORS & (~ GEO_BLANK_OUTPUT);
    GPS_ERRORS = GPS_ERRORS & (~ GEO_NO_SAT_IN_VIEW);
}

/**
 * Convert the gps coordinate of the format DDDMM.MMMM to one in R (real number).
 */
double convertGPS(double gps)
{
    double dec = (int)(gps/100);
    double flt = gps/100 - dec;
    flt *= 100;
    double result = dec + flt/60;
    return result;
}


/**
 * Gets a sentence from GPS sensor and parse it accordingly to set
 * the lat, lng, heading, and speed and generates appropriate
 * messages for satellites infomation.
 * @param sentence The sentence to be parsed.
 * @param lat The latitude to be set.
 * @param lng The longitude to be set.
 * @param heading The heading to be set.
 * @param speed The speed to be set.
 *
 */
int parseLL(char* sentence, double &lat, double &lng, double &heading, double &speed)
{
    static bool found_vector = false;
    char ns, ew;                 // direction.
    double latitude = LL_UNDEF, longitude = LL_UNDEF;
    int gpsQuality, numOfSatInView, numOfSatInUse;
    char tmpc;                   //store garbage.
    int tmpi;                    //store garbage.
    double tmpf;                 //store garbage
    int success = 0;
    static POINT pt_cur, pt_last;
    int gpsdate, gpstime;
    stringstream ss;
    string gpstimestr;

    if(!checkSumForSentence(sentence))
    {
        cerr << "WARN:GPS:Check sum for the sentence '" <<  sentence << "' failed" << endl;
        success = 1;
    }
    // only some GPS output has latitude and longitude. Parse these sentences only.
    else if (strncmp(sentence, "$GPGLL", 6) == 0)
    {
        if (sscanf (sentence,"$GPGLL,%lf,%c,%lf,%c", &latitude, &ns, &longitude, &ew) == 4)
        {
            lat = (double)latitude;
            lng = (double)longitude;
        }
        else
        {
            cerr << "WARN:GPS:Failed to parse GPGLL line `" << sentence << "`" << endl;
            success = 1;
        }
    }
    else if (strncmp(sentence, "$GPRMC", 6) == 0)
    {
        if (sscanf(sentence,"$GPRMC,%d,%c,%lf,%c,%lf,%c,%lf,%lf,%d", &gpstime, &tmpc, &latitude, &ns, &longitude, &ew, &tmpf, &tmpf, &gpsdate) == 9 ||
            sscanf(sentence,"$GPRMC,%d,%c,%lf,%c,%lf,%c,,,%d", &gpstime, &tmpc, &latitude, &ns, &longitude, &ew, &gpsdate) == 7)
        {
            lat = (double)latitude ;
            lng = (double)longitude;
            if(!get_valid_gps_time)
            {
              struct tm t;
              ss.str("");
              ss<< gpsdate<< gpstime;
              string gpstimestr = ss.str();
              strptime(gpstimestr.c_str(), "%d%m%y%H%M%S", &t);
              time_t gpstime_in_seconds = mktime(&t);
              gpstime_in_seconds -= 7*3600; //Set to PDT from UTC.
              ss.str("");
              ss<< gpstime_in_seconds;
              strptime(ss.str().c_str(), "%s", &t);
              char buf[80];
              strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
              if (gpstime_in_seconds > current_timeval.tv_sec - 1 && gpstime_in_seconds < future_time_t)
              {//set the system time using the gps time.
                  get_valid_gps_time = true;
                  ss.str("");
                  ss<< buf;
                  string command = "sudo date -s \"" + ss.str() + "\"";
                  int rc = system(command.c_str());
                  if(rc == 0)
                      cout<< "MSG:GPS:Successfully set the system clock using the GPS time"<< endl;
                  else
                      cerr<< "WARN:GPS:Failed to set the system clock using the GPS time"<< endl;
              }
            }
        }
        else
        {
            cerr << "WARN:GPS:Failed to parse GPRMC line: `" << sentence << "` " << endl;
            success = 1;
        }
    }
    //GPS sentences for the satellites information.
    else if (strncmp(sentence, "$GPGGA", 6) == 0)
    {
        if (sscanf (sentence,"$GPGGA,%d,%lf,%c,%lf,%c,%d,%d", &tmpi, &latitude, &ns, &longitude, &ew, &gpsQuality, &numOfSatInUse) == 7)
        {
            lat = (double)latitude ;
            lng = (double)longitude ;
            if(numOfSatInUse == 0)
            {
                ResetAllErrors();
                GPS_ERRORS = GPS_ERRORS | GEO_NO_SAT_IN_USE;
            }
            else if(gpsQuality == 0)
            {
                ResetAllErrors();
                GPS_ERRORS = GPS_ERRORS | GEO_NO_CONNECTION;
            }
            else if(gpsQuality == 6)
            {
                ResetAllErrors();
                GPS_ERRORS = GPS_ERRORS | GEO_ESTIMATED;
            }
            else if(gpsQuality == 1 || gpsQuality == 2)
            {
                ResetAllErrors();
            }
        }
        else
        {
            cerr << "WARN:GPS:Failed to parse GPGGA line: `" << sentence << "`" << endl;
            success = 1;
        }
    }
    else if (strncmp(sentence, "$GPGSV", 6) == 0)
    {
        if (sscanf(sentence,"$GPGSV,%d,%d,%d", &tmpi, &tmpi, &numOfSatInView) == 3)
        {
            if(numOfSatInView == 0)
                GPS_ERRORS = GPS_ERRORS | GEO_NO_SAT_IN_VIEW;
            else
                GPS_ERRORS = GPS_ERRORS & (~ GEO_NO_SAT_IN_VIEW);
        }
    }
    /*
    //GPS sentence for heading & speed information.
    else if (strncmp(sentence, "$GPVTG", 6) == 0)
    {
        if (sscanf(sentence,"$GPVTG,%lf,T,%d,M,%lf,N,%lf", &heading, &tmpi, &speed, &tmpf) == 4)
        {
            cout << "MSG:GPS:HEADING="<< heading<< ",SPEED="<< speed<< endl;
            found_vector = true; // found heading and speed from sensor, stop calculating it.
        }
    }
    */

    if (success == 0)
    {
        // only update lat/lng if it was a spatial line.
        if (latitude != LL_UNDEF) {
            lng = convertGPS(lng);
            lat = convertGPS(lat);
            if (ew == 'W') lng=-lng;
            if (ns == 'S') lat=-lat;
        }
        // If it's not in the GPS output, calculate heading and speed.
        if (!found_vector)
        {
            memcpy(&pt_last,&pt_cur,sizeof(POINT));
            pt_cur.x = lng;
            pt_cur.y = lat;
            speed = ll2distance(pt_cur,pt_last);
            heading = ll2heading(pt_cur,pt_last);
        }

    }
    return success;
}


char GPS_PARTIAL_LINE[255] = ""; ///< Store the partial lines from the GPS sensor. 

/**
 *	Check the first two characters match those of a valid Garmin GPS sentence.
 */
bool isSentence(char* sentence)
{
    return  strlen(sentence) > 6
        && ( strstr(sentence,"$GP") == sentence
        || strstr(sentence,"$GP") == sentence);
}


/**
 * A handwritten parser for various GPS output protocols, allowing for arbitrary mid-line breaks.
 * The latitude, longitude, heading, and speed info will be extracted from the lines in the buffer.
 * @param buf The buffer contains GPS info from the sensor.
 */
int parseGPS(char* buf, double &lat, double &lng, double &heading, double &speed)
{

    char *tok = NULL;
    char *ltok = NULL;
    int line_num = 0;

    GPS_ERRORS = 0;              //clear all the errors.

    // tokenize by newlines.
    tok = strtok (buf,"\n");

    gettimeofday(&current_timeval, NULL);
    struct tm future_t;
    strptime("2099-01-01 00:00:00", "%Y-%m-%d %H:%M:%S", &future_t);
    future_time_t = mktime(&future_t);

    while (1)
    {
        // we need to look ahead to the next line in order to process the current one.
        ltok = tok;
        tok = strtok (NULL, "\n");

        if (tok == NULL)         // if tok is the last line in the buffer...
        {

            if (ltok)
            {
                // copy partial token to beginning of buffer.
                if (line_num == 0)
                {
                    if (isSentence(ltok))
                    {
                        parseLL(GPS_PARTIAL_LINE, lat, lng, heading, speed);
                        strcpy(GPS_PARTIAL_LINE, ltok);
                    }
                    else
                    {
                        strcat(GPS_PARTIAL_LINE, ltok);
                    }
                }
                else if (line_num > 0)
                {
                    strcpy(GPS_PARTIAL_LINE, ltok);
                }
                CounterForBlankOutput = 0;
            }
            else                 // Warn if the GPS stops outputting anything.
            {
                CounterForBlankOutput++;
                if(CounterForBlankOutput > 5)
                {
                    cerr << "GPS:GPS output is blank" << endl;
                    GPS_ERRORS = GPS_ERRORS | GEO_BLANK_OUTPUT;
                }
            }
            break;

        }                        // if ltok is the first token...
        else if (line_num == 0)
        {

            // handle partial sentences left over from previous scan.
            if (isSentence(ltok))// the partial line buf was a full sentence. parse it, and then parse ltok.
            {
                if (isSentence(GPS_PARTIAL_LINE))
                {
                    parseLL(GPS_PARTIAL_LINE, lat, lng, heading, speed);
                    GPS_PARTIAL_LINE[0] = 0;
                }
                parseLL(ltok, lat, lng, heading, speed);
            }
            else
            {
                                 // the partial line is combined with tok and parsed.
                strcat(GPS_PARTIAL_LINE, ltok);
                parseLL(GPS_PARTIAL_LINE, lat, lng, heading, speed);
            }
            CounterForBlankOutput = 0;

        }                        // finally, if none of the above apply parse the previous line.
        else if (line_num > 0)
        {
            parseLL(ltok, lat, lng, heading, speed);
            CounterForBlankOutput = 0;
        }
        // do nothing for first line.

        line_num ++;
    }

    return GPS_ERRORS;
}


double ll2distance(POINT p1, POINT p2)
{
    double earthRadius = 3958.75;
    double dLat = (p2.y-p1.y) * PI / 180;
    double dLng = (p2.x-p1.x) * PI / 180;
    double a = sin(dLat/2) * sin(dLat/2) +
        cos(p1.y * PI / 180) * cos(p2.y * PI/180) *
        sin(dLng/2) * sin(dLng/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    double dist = earthRadius * c;

    int meterConversion = 1609;
   
    return abs(dist * meterConversion);
}


double ll2heading(POINT p1, POINT p2)
{
    double result = atan2(p1.y - p2.y, p1.x - p2.x) * 180 / PI + 360;
    int int_part = (int)result;

    return int_part % 360 + result - int_part;
}


