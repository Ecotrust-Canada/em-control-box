/*
VMS: Vessel Monitoring System - Control Box Software
Copyright (C) 2011 Ecotrust Canada
Knowledge Systems and Planning

This file is part of VMS.

VMS is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

VMS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with VMS. If not, see <http://www.gnu.org/licenses/>.

You may contact Ecotrust Canada via our websitehttp://ecotrust.ca
*/

#include<iostream>
#include<fstream>
#include<string>
#include<sstream>
using namespace std;



string toUpperCase(string s)
{
    string result(s);
    for(int i = 0; i< s.length(); i++)
    {
        result[i] = toupper(s[i]);
    }
    return result;
}

string checksum(string sentence)
{
    int tmp= sentence[1];
    int i = 2;

    //According to the documentation, the checksum for the GPS sentence is specially checked,
    //i.e., compute xor of all the bits in the sentence.
    for(; i< sentence.length(); i++)
    {
        tmp = tmp ^ sentence[i];
    }

    char tre[10];
    sprintf(tre, "%x", tmp);  //convert to hex.
    string result(tre);
    result = toUpperCase(result);
    return result;
}

string itoh(int isum)
{
    static stringstream ss;
    string result;
    ss.clear();
    ss<< hex<< isum;
    ss>> result;
    return result;
}


string ltoh(long num)
{
    static stringstream ss;
    string result;
    ss.clear();
    ss<< hex<< num;
    ss>> result;
    return result;
}


string checkSumRFID(string s)
{
    int isum = 0;
    for (int i = 0; i< s.length(); i++)
    {
        isum += s[i];
    }
    return itoh(isum % 256);
}

string toHexRFID(long id)
{
    string hid = ltoh(id);
    while(hid.length() < 10)
        hid = "0" + hid;
    
    return hid;
}

int main()
{
    //$GPRMC,122251,V,4916.8514,N,12306.2840,W,,,010211,018.9,E*7D
    //01AF36BED24E
    //P819 B460 for high pressure and P250 B460 for low pressure
    ofstream GPSOut("GPS.in");
    ofstream RFIDOut("RFID.in");
    ofstream PSIOut("PSI.in");
    GPSOut<< '\0'<< endl;
    RFIDOut<< '\0'<< endl;
    PSIOut<< '\0'<< endl;
    int lat = 5000, lng = 12306;
    string RFIDs[4];
    RFIDs[0] = toHexRFID(488769590);
    RFIDs[1] = toHexRFID(438293501);
    RFIDs[2] = toHexRFID(438293436);
    RFIDs[3] = toHexRFID(438293434);
    
    for(int i = lat; i<36000; i+=100)
    {
        char tmp[100];
        sprintf(tmp, "$GPRMC,122251,V,%d.1234,N,%d.5678,W,,,", i, lng);
        string sentence(tmp);
        sentence = sentence + "*" + checksum(sentence);
        GPSOut<< sentence<< endl<< '\0';
        if(i == 12500 || i == 20000 || i == 28000 || i == 32000 || i == 35000)
        {
            PSIOut<< "P819"<< endl<< "B460"<< endl<< '\0';
            RFIDOut<< '\0';
            for(int k = 0; k< 4; k++)
            {
                for(int j = 0; j< 10; j++)
                {
                    GPSOut<< sentence<< endl<< '\0';
                    PSIOut<< "P819"<< endl<< "B460"<< endl<< '\0';
                    if(i != 35000 && j == 5)
                        RFIDOut<< char(0x3A)<< RFIDs[k]<< checkSumRFID(RFIDs[k])<< char(0x0D)<< '\0';
                    else
                        RFIDOut<< '\0';
                }
                
                if(i == 32000 || i == 35000)
                    break;
                
                if(k != 3)
                {
                    i+= 100;
                    for(int j = 0; j < 10; j++)
                    {
                        char tmp[100];
                        sprintf(tmp, "$GPRMC,122251,V,%d.1234,N,%d.5678,W,,,", i, lng);
                        string innerSentence(tmp);
                        innerSentence = innerSentence + "*" + checksum(innerSentence);
                        GPSOut<< innerSentence<< endl<< '\0';
                        sentence = innerSentence;
                        PSIOut<< "P250"<< endl<< "B460"<< endl<< '\0';
                        RFIDOut<< '\0';
                        i+= 100;
                    }
                }
            }

        }
        else if(i == 35500)
        {
            RFIDOut<< char(0x3A)<< RFIDs[1]<< checkSumRFID(RFIDs[1])<< char(0x0D)<< '\0';
            PSIOut<< "P250"<< endl<< "B460"<< endl<< '\0';
        }
        else
        {
            PSIOut<< "P250"<< endl<< "B460"<< endl<< '\0';
            RFIDOut<< '\0';
        }
    }

    GPSOut.close();
    PSIOut.close();
    RFIDOut.close();

    return 0;
}

