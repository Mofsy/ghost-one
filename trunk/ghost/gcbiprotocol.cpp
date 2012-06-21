/*

   Copyright 2010 Trevor Hogan

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/

#include "ghost.h"
#include "util.h"
#include "gcbiprotocol.h"

//
// CGCBIProtocol
//

CGCBIProtocol :: CGCBIProtocol( )
{

}

CGCBIProtocol :: ~CGCBIProtocol( )
{

}

///////////////////////
// RECEIVE FUNCTIONS //
///////////////////////

CIncomingGarenaUser *CGCBIProtocol :: RECEIVE_GCBI_INIT( BYTEARRAY data )
{
	// 2 bytes					-> Header
	// 2 bytes					-> Length
	// 4 bytes					-> actual IP address (big endian)
	// 4 bytes					-> Garena user ID (big endian)
	// 4 bytes					-> Garena room ID (big endian)
	// 4 bytes					-> Garena user experience (big endian)
	// 2 bytes					-> country string from Garena

	if( ValidateLength( data ) && data.size( ) == 22 )
	{
		BYTEARRAY IP = BYTEARRAY( data.begin( ) + 4, data.begin( ) + 8 );
		BYTEARRAY UserID = BYTEARRAY( data.begin( ) + 8, data.begin( ) + 12 );
		BYTEARRAY RoomID = BYTEARRAY( data.begin( ) + 12, data.begin( ) + 16 );
		BYTEARRAY UserExp = BYTEARRAY( data.begin( ) + 16, data.begin( ) + 20 );
		BYTEARRAY Country = BYTEARRAY( data.begin( ) + 20, data.begin( ) + 22 );
		
		return new CIncomingGarenaUser(UTIL_ByteArrayToUInt32( IP, true ),
											UTIL_ByteArrayToUInt32( UserID, true ),
											UTIL_ByteArrayToUInt32( RoomID, true ),
											UTIL_ByteArrayToUInt32( UserExp, true ),
											string( Country.begin( ), Country.end( ) ) );
	}

	return NULL;
}

////////////////////
// SEND FUNCTIONS //
////////////////////

/////////////////////
// OTHER FUNCTIONS //
/////////////////////

bool CGCBIProtocol :: AssignLength( BYTEARRAY &content )
{
	// insert the actual length of the content array into bytes 3 and 4 (indices 2 and 3)

	BYTEARRAY LengthBytes;

	if( content.size( ) >= 4 && content.size( ) <= 65535 )
	{
		LengthBytes = UTIL_CreateByteArray( (uint16_t)content.size( ), false );
		content[2] = LengthBytes[0];
		content[3] = LengthBytes[1];
		return true;
	}

	return false;
}

bool CGCBIProtocol :: ValidateLength( BYTEARRAY &content )
{
	// verify that bytes 3 and 4 (indices 2 and 3) of the content array describe the length

	uint16_t Length;
	BYTEARRAY LengthBytes;

	if( content.size( ) >= 4 && content.size( ) <= 65535 )
	{
		LengthBytes.push_back( content[2] );
		LengthBytes.push_back( content[3] );
		Length = UTIL_ByteArrayToUInt16( LengthBytes, false );

		if( Length == content.size( ) )
			return true;
	}

	return false;
}

//
// CIncomingGarenaUser
//

CIncomingGarenaUser :: CIncomingGarenaUser( uint32_t nIP, uint32_t nUserID, uint32_t nRoomID, uint32_t nUserExp, string nCountryCode ) : m_IP( nIP ), m_UserID( nUserID ), m_RoomID( nRoomID ), m_UserExp( nUserExp ), m_CountryCode( nCountryCode )
{

}

CIncomingGarenaUser :: ~CIncomingGarenaUser( )
{

}
