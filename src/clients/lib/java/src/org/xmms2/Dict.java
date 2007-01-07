/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

package org.xmms2;

import java.util.HashMap;

/**
 * This class should reflect xmms2's dict's (not propdicts). It's more or
 * less a standard HashMap, only difference are the putDictEntry() and
 * getEntry methods which expect and return a String
 */

public class Dict extends HashMap {
	private static final long serialVersionUID = 6680740765293764269L;

	public void putDictEntry(String key, String value){
		super.put(key, value);
	}
	
	public Object put(Object key, Object value){
		putDictEntry(""+key, ""+value);
		return value;
	}
	
	public String getDictEntry(String key){
		return ""+get(key);
	}
}
