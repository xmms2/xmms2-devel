/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/helpers.h>

#include <string>

namespace Xmms
{

	/** Decode an URL-encoded string.
	 *
	 * Some strings (currently only the url of media) have no known
	 * encoding, and must be encoded in an UTF-8 clean way. This is done
	 * similarly to the url encoding web browsers do. This functions decodes
	 * a string encoded in that way. OBSERVE that the decoded string HAS
	 * NO KNOWN ENCODING and you cannot display it on screen in a 100%
	 * guaranteed correct way (a good heuristic is to try to validate the
	 * decoded string as UTF-8, and if it validates assume that it is an
	 * UTF-8 encoded string, and otherwise fall back to some other
	 * encoding).
	 *
	 * Do not use this function if you don't understand the
	 * implications. The best thing is not to try to display the url at
	 * all.
	 *
	 * Note that the fact that the string has NO KNOWN ENCODING and CAN
	 * NOT BE DISPLAYED does not stop you from open the file if it is a
	 * local file (if it starts with "file://").
	 *
	 * @param encoded_url the url-encoded string
	 * @return the decoded string in an unknown encoding
	 *
	 */
	std::string decodeUrl( const std::string& encoded_url )
	{
		xmmsv_t *encoded, *decoded;
		const unsigned char *url;
		unsigned int len;
		std::string dec_str;

		encoded = xmmsv_new_string( encoded_url.c_str() );
		decoded = xmmsv_decode_url( encoded );
		if( !xmmsv_get_bin( decoded, &url, &len ) ) {
			throw invalid_url( "The given URL cannot be decoded." );
		}

		dec_str = std::string( reinterpret_cast< const char* >( url ), len );

		xmmsv_unref( encoded );
		xmmsv_unref( decoded );

		return dec_str;
	}

}
