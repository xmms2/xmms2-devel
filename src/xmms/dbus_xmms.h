#ifndef __DBUS_XMMS_H__
#define __DBUS_XMMS_H__


/* Playback msgs */
#define XMMS_DBUS_SIGNAL_PLAYBACK_PLAY "org.xmms.playback.play"
#define XMMS_DBUS_SIGNAL_PLAYBACK_STOP "org.xmms.playback.stop"
#define XMMS_DBUS_SIGNAL_PLAYBACK_PAUSE "org.xmms.playback.pause"
#define XMMS_DBUS_SIGNAL_PLAYBACK_NEXT "org.xmms.playback.next"
#define XMMS_DBUS_SIGNAL_PLAYBACK_PREV "org.xmms.playback.prev"
#define XMMS_DBUS_SIGNAL_PLAYBACK_SEEK "org.xmms.playback.seek"
#define XMMS_DBUS_SIGNAL_PLAYBACK_CURRENTID "org.xmms.playback.currentid"
#define XMMS_DBUS_SIGNAL_PLAYBACK_PLAYTIME "org.xmms.playback.playtime"

/* Playlist msgs */
#define XMMS_DBUS_SIGNAL_PLAYLIST_ADD "org.xmms.playlist.add"
#define XMMS_DBUS_SIGNAL_PLAYLIST_REMOVE "org.xmms.playlist.remove"
#define XMMS_DBUS_SIGNAL_PLAYLIST_LIST "org.xmms.playlist.list"
#define XMMS_DBUS_SIGNAL_PLAYLIST_SHUFFLE "org.xmms.playlist.shuffle"
#define XMMS_DBUS_SIGNAL_PLAYLIST_CLEAR "org.xmms.playlist.clear"
#define XMMS_DBUS_SIGNAL_PLAYLIST_JUMP "org.xmms.playlist.jump"
#define XMMS_DBUS_SIGNAL_PLAYLIST_MEDIAINFO "org.xmms.playlist.mediainfo"
#define XMMS_DBUS_SIGNAL_PLAYLIST_MOVE "org.xmms.playlist.move"
#define XMMS_DBUS_SIGNAL_PLAYLIST_CHANGED "org.xmms.playlist.change"

/* Core msgs */
#define XMMS_DBUS_SIGNAL_CORE_QUIT "org.xmms.core.quit"
#define XMMS_DBUS_SIGNAL_CORE_DISCONNECT "org.freedesktop.Local.Disconnect"
#define XMMS_DBUS_SIGNAL_CORE_INFORMATION "org.xmms.core.information"
#define XMMS_DBUS_SIGNAL_CORE_SIGNAL_REGISTER "org.xmms.core.signal.register"
#define XMMS_DBUS_SIGNAL_CORE_SIGNAL_UNREGISTER "org.xmms.core.signal.unregister"

#endif /* __DBUS_XMMS_H__ */
