/*

   Copyright [2008] [Trevor Hogan]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#ifndef INCLUDES_H
#define INCLUDES_H

// standard integer sizes for 64 bit compatibility

#ifdef WIN32
 #include "ms_stdint.h"
#else
 #include <stdint.h>
#endif

// STL

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

using namespace std;

typedef vector<unsigned char> BYTEARRAY;
typedef pair<unsigned char,string> PIDPlayer;


/*
#ifdef WIN32
#include <windows.h>
#include <wininet.h>
#include <winsock.h>
#endif
*/

// time

uint32_t GetTime( );		// seconds
uint32_t GetTicks( );		// milliseconds

#ifdef WIN32
 #define MILLISLEEP( x ) Sleep( x )
#else
 #define MILLISLEEP( x ) usleep( ( x ) * 1000 )
#endif

// network

#undef FD_SETSIZE
#define FD_SETSIZE 512

// output

void CONSOLE_Print( string message );
void DEBUG_Print( string message );
void DEBUG_Print( BYTEARRAY b );

// channel

vector<string> Channel_Users ();
void Channel_Clear (string name);
void Channel_Add (string name);
void Channel_Join (string server, string name);
void Channel_Del (string name);

const uint32_t CMD_ban = 0;
const uint32_t CMD_delban = 1;
const uint32_t CMD_host = 2;
const uint32_t CMD_unhost = 3;
const uint32_t CMD_end = 4;
const uint32_t CMD_mute = 5;
const uint32_t CMD_kick = 6;
const uint32_t CMD_say = 7;
const uint32_t CMD_open = 8;
const uint32_t CMD_close = 9;
const uint32_t CMD_swap = 10;
const uint32_t CMD_sp = 11;
const uint32_t CMD_quit = 12;
const uint32_t CMD_mod = 13;
const uint32_t CMD_admin = 14;
const string CMD_string = "ban delban host unhost end mute kick say open close swap sp quit";
const string CMD_stringshow = "ban delban host unhost end mute kick say open close swap sp quit";

// command access

bool CMDCheck (uint32_t cmd, uint32_t acc);
uint32_t CMDAccessAll ();

// path fix

string FixPath(string Path, string End);

// patch 21

bool Patch21();

#endif
