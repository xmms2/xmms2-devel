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
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;

/**
 * Class is usually only used once or on updates replaced by another instance. It holds a list
 * of Titles, the current index, ...
 */
public class Playlist {
	private List pl;
	private HashMap titles;
	private int position = 0;
	private Xmms2 xmms2;
	private int lastAction = -1;
	
	/**
	 * 
	 * @param xmms2
	 */
	protected Playlist(Xmms2 xmms2){
		this.xmms2 = xmms2;
		pl = new ArrayList();
		titles = new HashMap();
		xmms2.playlistListAsync();
		xmms2.playlistGetCurrentIndexAsync();
	}
	
	public int add(long id){
		return xmms2.addId(id);
	}
	
	public int addURL(String url){
		return xmms2.addUrl(url);
	}
	
	public int insertTitle(long id, int index){
		return xmms2.insertId(id, index);
	}
	
	public int insertTitle(String url, int index){
		return xmms2.insertUrl(url, index);
	}
	
	public int move(int source, int dest){
		return xmms2.move(source, dest);
	}
	
	public String toString(){
		String output = "";
		int i = 1;
		for (ListIterator it = pl.listIterator(); it.hasNext();){
			Long t = (Long)it.next();
			output += i++ + "\t" + t + "\n";
		}
		return output;
	}
	
	public ListIterator listIterator(){
		return pl.listIterator();
	}
	
	public Iterator Iterator(){
		return pl.iterator();
	}
	
	/**
	 * @param t
	 * @return		index of Title t
	 */
	public int indexOf(Title t){
		return pl.indexOf(t);
	}
	
	/**
	 * remove Title from playlist
	 * @param t
	 * @return		removed Title
	 */
	public Title removeTitle(Title t){
		xmms2.removeIndex(indexOf(t));
		return t;
	}
	
	/**
	 * Check if this playlist contains the given id
	 * @param id
	 * @return		true if yes, false otherwise
	 */
	public List indicesOfID(long id){
		ArrayList indices = new ArrayList();
		int i = 0;
		for (ListIterator it = pl.listIterator(); it.hasNext();){
			if (((Long)it.next()).longValue() == id) indices.add(new Integer(i));
			i++;
		}
		return indices;
	}
	
	/**
	 * @param index
	 * @return		Title on index
	 */
	public Title titleAt(int index) {
		Object o = titles.get(""+pl.get(index));
		Title t = null;
		if (o instanceof Title)
			t = (Title)o;
		return t;
	}
	
	public long idAt(int index){
		return ((Long)pl.get(index)).longValue();
	}
	
	/**
	 * 
	 * @return		size of Playlist
	 */
	public int length(){
		return pl.size();
	}
	
	/**
	 * 
	 * @return		current position
	 */
	public int getPosition(){
		return position;
	}
	
	/**
	 * @return		current marked Title		
	 */
	public Title getCurrentTitle(){
		return titleAt(position);
	}
	
	/**
	 * set next
	 * @param pos
	 * @return td
	 */
	public int setNextAbs(int pos){
		if (pos > pl.size()-1)
			return xmms2.setNextAbs(pos);
		return xmms2.setNextAbs(pos);
	}
	
	/**
	 * set next
	 * @param pos
	 * @return tid
	 */
	public int setNextRel(int pos){
		if (pos > pl.size()-1)
			return xmms2.setNextRel(pos);
		return xmms2.setNextRel(pos);
	}
	
	/**
	 * clear playlist
	 * @return tid
	 *
	 */
	public int clear(){
		titles.clear();
		return xmms2.clear();
	}
	
	/**
	 * shuffle playlist
	 * @return tid
	 *
	 */
	public int shuffle(){
		return xmms2.shuffle();
	}
	
	/**
	 * sort playlist
	 * @param by 
	 * @return tid
	 *
	 */
	public int sort(String by){
		return xmms2.sort(by);
	}
	
	public int remove(int index){
		return xmms2.removeIndex(index);
	}
	
	public void remove(int indices[]){
		for ( int i = 0; i < indices.length; i++)
			remove(indices[i]-i);
	}
	
	public int getLastAction(){
		return lastAction;
	}
	
	protected void setLastAction(int lastAction){
		this.lastAction = lastAction;
	}
	
	protected void updateList(List l){
		pl = l;
	}
	
	protected void updatePosition(long pos){
		position = (int)pos;
	}
	
	protected void updateTitle(Title t){
		titles.put(""+t.getID(), t);
	}
}
