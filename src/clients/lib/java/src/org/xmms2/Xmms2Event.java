/******************************************************************
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Copyright:
 * Georg Schild (dangertools@gmail.com)
 * Franz Endstrasser (franz.endstrasser@gmail.com)
 ******************************************************************/

package org.xmms2;

/**
 * Events are moved to the Xmms2Listeners. possible types are listed in Xmms2Listener
 */

public class Xmms2Event {
	public long tid = 0;
	public String type = Xmms2Listener.VOID_TYPE;
	public Object value = null;
	
	protected Xmms2Event(long tid, String type, Object value){
		this.tid = tid;
		this.type = type;
		this.value = value;
		
		if (type.equals(Xmms2Listener.ERROR_TYPE))
			System.err.println(value);
	}
	
	public String toString(){
		return "" + value;
	}
}
