#ifndef __SIGNAL_XMMS_H__
#define __SIGNAL_XMMS_H__


/* Playback msgs */
#define XMMS_SIGNAL_PLAYBACK_PLAY "org.xmms.playback.play"
#define XMMS_SIGNAL_PLAYBACK_STOP "org.xmms.playback.stop"
#define XMMS_SIGNAL_PLAYBACK_PAUSE "org.xmms.playback.pause"
#define XMMS_SIGNAL_PLAYBACK_NEXT "org.xmms.playback.next"
#define XMMS_SIGNAL_PLAYBACK_PREV "org.xmms.playback.prev"
#define XMMS_SIGNAL_PLAYBACK_SEEK_MS "org.xmms.playback.seek.ms"
#define XMMS_SIGNAL_PLAYBACK_SEEK_SAMPLES "org.xmms.playback.seek.samples"
#define XMMS_SIGNAL_PLAYBACK_CURRENTID "org.xmms.playback.currentid"
#define XMMS_SIGNAL_PLAYBACK_PLAYTIME "org.xmms.playback.playtime"

/* Playlist msgs */
#define XMMS_SIGNAL_PLAYLIST_ADD "org.xmms.playlist.add"
#define XMMS_SIGNAL_PLAYLIST_REMOVE "org.xmms.playlist.remove"
#define XMMS_SIGNAL_PLAYLIST_LIST "org.xmms.playlist.list"
#define XMMS_SIGNAL_PLAYLIST_SHUFFLE "org.xmms.playlist.shuffle"
#define XMMS_SIGNAL_PLAYLIST_CLEAR "org.xmms.playlist.clear"
#define XMMS_SIGNAL_PLAYLIST_JUMP "org.xmms.playlist.jump"
#define XMMS_SIGNAL_PLAYLIST_MEDIAINFO "org.xmms.playlist.mediainfo"
#define XMMS_SIGNAL_PLAYLIST_MOVE "org.xmms.playlist.move"
#define XMMS_SIGNAL_PLAYLIST_CHANGED "org.xmms.playlist.change"
#define XMMS_SIGNAL_PLAYLIST_SAVE "org.xmms.playlist.save"

/* Core msgs */
#define XMMS_SIGNAL_CORE_QUIT "org.xmms.core.quit"
#define XMMS_SIGNAL_CORE_DISCONNECT "org.freedesktop.Local.Disconnect"
#define XMMS_SIGNAL_CORE_INFORMATION "org.xmms.core.information"
#define XMMS_SIGNAL_CORE_SIGNAL_REGISTER "org.xmms.core.signal.register"
#define XMMS_SIGNAL_CORE_SIGNAL_UNREGISTER "org.xmms.core.signal.unregister"

/* Transport msgs */
#define XMMS_SIGNAL_TRANSPORT_MIMETYPE "org.xmms.transport.mimetype"

/* Output msgs */
#define XMMS_SIGNAL_OUTPUT_EOS_REACHED "org.xmms.output.eos.reached"

/* Visualisation msgs */
#define XMMS_SIGNAL_VISUALISATION_SPECTRUM "org.xmms.visualisation.spectrum"

/* Config msgs */
#define XMMS_SIGNAL_CONFIG_SAVE "org.xmms.config.save"
#define XMMS_SIGNAL_CONFIG_VALUE_CHANGE "org.xmms.config.value.change"

#endif /* __SIGNAL_XMMS_H__ */
