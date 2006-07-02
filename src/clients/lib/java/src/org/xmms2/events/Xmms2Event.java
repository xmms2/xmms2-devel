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

package org.xmms2.events;


/**
 * Events are moved to the Xmms2Listeners. possible types are listed in
 * Xmms2Listener
 */

public class Xmms2Event {
    public int tid = 0;

    public String type = Xmms2Listener.VOID_TYPE;

    public Object value = null;

    public Xmms2Event(int tid, String type, Object value) {
        this.tid = tid;
        this.type = type;
        this.value = value;

        if (type.equals(Xmms2Listener.ERROR_TYPE))
            System.err.println(value);
    }
    
    public Xmms2Event(long tid, String type, Object value){
    	this((int)tid, type, value);
    }

    public String toString() {
        return "" + value;
    }
}
