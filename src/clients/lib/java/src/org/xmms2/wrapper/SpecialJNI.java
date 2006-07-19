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

package org.xmms2.wrapper;

import java.io.FileDescriptor;

/**
 * This class contains functions needed to handle special java needs
 */

public class SpecialJNI {

    /**
     * You have to call this function <b><u>before</u></b> you can use the
     * callbacks, or better before the callbacks can call your implemented
     * functions
     * 
     * @param objectName
     *            An object which implements the CallbacksListener interface
     */
    public final static native void setENV(CallbacksListener objectName);

    /**
     * This function is called by the mainloop, should therefor not be called by
     * the user
     * 
     * @param mainloop
     *            A JMain object
     * @param connection pointer to a connection
     */
    protected final static native void setupMainloop(Object mainloop,
            long connection);

    /**
     * This method "converts" a filedescriptor gotten from xmmsc_io_fd_get() to
     * a java FileDescriptor object
     * 
     * @param fd
     *            FileDescriptor object
     * @param c
     *            pointer to xmmsc_connection_St (use
     *            org.xmms2.xmms2bindings.Xmmsclient.getPointerToConnection())
     */
    public final static native void getFD(FileDescriptor fd, long c);
}
