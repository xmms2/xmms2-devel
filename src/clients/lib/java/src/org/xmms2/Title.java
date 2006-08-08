/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

import java.util.Arrays;
import java.util.List;


public class Title implements Comparable {
    private PropDict attributes;
    private long id = -1;
    
    public Title() {
        attributes = new PropDict();
    }

    /**
     * Adds/Sets a key to a specified value. keys are stored and received
     * lowercase
     * 
     * @param key
     * @param value
     * @param source 
     */
    public void setAttribute(String key, String value, String source) {
        if (value == null)
            value = "";
        if (key != null)
            attributes.putPropDictEntry(key.toLowerCase(), value, source);
    }

    public List getAttribute(String key) {
        return attributes.getEntry(key.toLowerCase());
    }
    
    public PropDictEntry getFirstAttribute(String key){
    	List l = getAttribute(key);
    	if ( l != null && l.size() > 0)
    		return (PropDictEntry)l.get(0);
    	return new PropDictEntry("", "");
    }
    
    public long getID(){
    	return id;
    }
    
    public void setID(long id){
    	this.id = id;
    }

    public PropDict getAttributes() {
        return attributes;
    }

    public String toString() {
        String out = "";
        String keys[] = (String[]) attributes.keySet().toArray(new String[0]);
        Arrays.sort(keys);
        for (int i = 0; i < keys.length; i++) {
        	PropDictEntry p = (PropDictEntry)attributes.get(keys[i]);
            out += keys[i] + "\t" + p.getValue() + "\t[" + p.getSource() + "]\n";
        }
        return out;
    }

    public boolean equals(Object o) {
        if (!(o instanceof Title))
            return false;
        Title t = (Title) o;
        return t.getID() == getID();
    }

    public int compareTo(Object arg0) {
        if (arg0 instanceof Title) {
            Title t = (Title) arg0;
            return new Long(getID()).compareTo(new Long(t
                .getID()));
        }
        return 0;
    }
}
