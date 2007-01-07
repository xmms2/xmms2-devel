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

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * This class should reflect xmms2's propdicts (not plain dict). It is more
 * or less a simple HashMap with the difference that it sores 3 values
 * and returns a List or PropDictEntries
 */

public class PropDict extends HashMap {
	private static final long serialVersionUID = 6171320128340947211L;

	public void putPropDictEntry(String key, String value, String source){
		List l = (List)get(key);
		if (l == null)
			l = new ArrayList();
		l.add(new PropDictEntry(value, source));
		super.put(key, l);
	}
	
	public List getEntry(String key){
		return (List)get(key);
	}
	
	public Object put(Object key, Object value){
		putPropDictEntry(""+key, ""+value, "");
		return value;
	}
}
