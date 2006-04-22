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
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

public class Title implements Comparable {
    private HashMap attributes;

    public Title() {
        attributes = new HashMap();
        setAttribute("artist", "[Unknown artist]");
        setAttribute("album", "[Unknown album]");
        setAttribute("title", "[Unknown title]");
    }

    /**
     * Adds/Sets a key to a specified value. keys are stored and received
     * lowercase
     * 
     * @param key
     * @param value
     */
    public void setAttribute(String key, String value) {
        if (value == null)
            value = "";
        if (key != null)
            attributes.put(key.toLowerCase(), value);
    }

    public String getAttribute(String key) {
        Object value = attributes.get(key.toLowerCase());
        if (value == null)
            value = "";
        return "" + value;
    }

    public Map getAttributes() {
        return attributes;
    }

    public String toString() {
        String out = "";
        String keys[] = (String[]) attributes.keySet().toArray(new String[0]);
        Arrays.sort(keys);
        for (int i = 0; i < keys.length; i++) {
            out += keys[i] + "\t" + attributes.get(keys[i]) + "\n";
        }
        return out;
    }

    public boolean equals(Object o) {
        if (!(o instanceof Title))
            return false;
        Title t = (Title) o;
        if (t.getAttributes().size() != getAttributes().size())
            return false;
        Iterator a = t.getAttributes().keySet().iterator();
        while (a.hasNext()) {
            String keyA = (String) a.next();
            String valA = t.getAttribute(keyA);
            String valB = getAttribute(keyA);
            if (!valA.equals(valB))
                return false;
        }
        return true;
    }

    public int compareTo(Object arg0) {
        if (arg0 instanceof Title) {
            Title t = (Title) arg0;
            try {
                return new Integer(getAttribute("id")).compareTo(new Integer(t
                        .getAttribute("id")));
            } catch (Exception e) {
            }
        }
        return 0;
    }
}
