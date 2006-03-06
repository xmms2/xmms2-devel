#ifndef XMMSCLIENTPP_METHODS_H
#define XMMSCLIENTPP_METHODS_H
XMMSResult *quit (void) { return new XMMSResult (xmmsc_quit (m_xmmsc)); }
XMMSResult *plugin_list (uint32_t x) { return new XMMSResult (xmmsc_plugin_list (m_xmmsc, x)); }
XMMSResult *main_stats (void) { return new XMMSResult (xmmsc_main_stats (m_xmmsc)); }
XMMSResult *broadcast_quit (void) { return new XMMSResult (xmmsc_broadcast_quit (m_xmmsc)); }
XMMSResult *playlist_shuffle (void) { return new XMMSResult (xmmsc_playlist_shuffle (m_xmmsc)); }
XMMSResult *playlist_add (const char * x) { return new XMMSResult (xmmsc_playlist_add (m_xmmsc, x)); }
XMMSResult *playlist_add_id (uint32_t x) { return new XMMSResult (xmmsc_playlist_add_id (m_xmmsc, x)); }
XMMSResult *playlist_remove (uint32_t x) { return new XMMSResult (xmmsc_playlist_remove (m_xmmsc, x)); }
XMMSResult *playlist_clear (void) { return new XMMSResult (xmmsc_playlist_clear (m_xmmsc)); }
XMMSResultValueList<uint> *playlist_list (void) { return new XMMSResultValueList<uint> (xmmsc_playlist_list (m_xmmsc)); }
XMMSResult *playlist_sort (char* x) { return new XMMSResult (xmmsc_playlist_sort (m_xmmsc, x)); }
XMMSResult *playlist_set_next (uint32_t x) { return new XMMSResult (xmmsc_playlist_set_next (m_xmmsc, x)); }
XMMSResult *playlist_set_next_rel (int32_t x) { return new XMMSResult (xmmsc_playlist_set_next_rel (m_xmmsc, x)); }
XMMSResult *playlist_move (uint32_t x,uint32_t y) { return new XMMSResult (xmmsc_playlist_move (m_xmmsc, x, y)); }
XMMSResult *playlist_current_pos (void) { return new XMMSResult (xmmsc_playlist_current_pos (m_xmmsc)); }
XMMSResult *playlist_insert (int x,char * y) { return new XMMSResult (xmmsc_playlist_insert (m_xmmsc, x, y)); }
XMMSResult *playlist_insert_id (int x,uint32_t y) { return new XMMSResult (xmmsc_playlist_insert_id (m_xmmsc, x, y)); }
XMMSResultDict *broadcast_playlist_changed (void) { return new XMMSResultDict (xmmsc_broadcast_playlist_changed (m_xmmsc)); }
XMMSResult *broadcast_playlist_current_pos (void) { return new XMMSResult (xmmsc_broadcast_playlist_current_pos (m_xmmsc)); }
XMMSResult *playback_stop (void) { return new XMMSResult (xmmsc_playback_stop (m_xmmsc)); }
XMMSResult *playback_tickle (void) { return new XMMSResult (xmmsc_playback_tickle (m_xmmsc)); }
XMMSResult *playback_start (void) { return new XMMSResult (xmmsc_playback_start (m_xmmsc)); }
XMMSResult *playback_pause (void) { return new XMMSResult (xmmsc_playback_pause (m_xmmsc)); }
XMMSResultValue<uint> *playback_current_id (void) { return new XMMSResultValue<uint> (xmmsc_playback_current_id (m_xmmsc)); }
XMMSResult *playback_seek_ms (uint32_t x) { return new XMMSResult (xmmsc_playback_seek_ms (m_xmmsc, x)); }
XMMSResult *playback_seek_ms_rel (int x) { return new XMMSResult (xmmsc_playback_seek_ms_rel (m_xmmsc, x)); }
XMMSResult *playback_seek_samples (uint32_t x) { return new XMMSResult (xmmsc_playback_seek_samples (m_xmmsc, x)); }
XMMSResult *playback_seek_samples_rel (int x) { return new XMMSResult (xmmsc_playback_seek_samples_rel (m_xmmsc, x)); }
XMMSResult *playback_playtime (void) { return new XMMSResult (xmmsc_playback_playtime (m_xmmsc)); }
XMMSResultValue<uint> *playback_status (void) { return new XMMSResultValue<uint> (xmmsc_playback_status (m_xmmsc)); }
XMMSResult *playback_volume_set (const char * x,uint32_t y) { return new XMMSResult (xmmsc_playback_volume_set (m_xmmsc, x, y)); }
XMMSResult *playback_volume_get (void) { return new XMMSResult (xmmsc_playback_volume_get (m_xmmsc)); }
XMMSResult *broadcast_playback_volume_changed (void) { return new XMMSResult (xmmsc_broadcast_playback_volume_changed (m_xmmsc)); }
XMMSResultValue<uint> *broadcast_playback_status (void) { return new XMMSResultValue<uint> (xmmsc_broadcast_playback_status (m_xmmsc)); }
XMMSResultValue<uint> *broadcast_playback_current_id (void) { return new XMMSResultValue<uint> (xmmsc_broadcast_playback_current_id (m_xmmsc)); }
XMMSResultValue<uint> *signal_playback_playtime (void) { return new XMMSResultValue<uint> (xmmsc_signal_playback_playtime (m_xmmsc)); }
XMMSResult *configval_set (char * x,char * y) { return new XMMSResult (xmmsc_configval_set (m_xmmsc, x, y)); }
XMMSResult *configval_list (void) { return new XMMSResult (xmmsc_configval_list (m_xmmsc)); }
XMMSResult *configval_get (char * x) { return new XMMSResult (xmmsc_configval_get (m_xmmsc, x)); }
XMMSResult *configval_register (char * x,char * y) { return new XMMSResult (xmmsc_configval_register (m_xmmsc, x, y)); }
XMMSResult *broadcast_configval_changed (void) { return new XMMSResult (xmmsc_broadcast_configval_changed (m_xmmsc)); }
XMMSResult *broadcast_mediainfo_reader_status (void) { return new XMMSResult (xmmsc_broadcast_mediainfo_reader_status (m_xmmsc)); }
XMMSResult *signal_visualisation_data (void) { return new XMMSResult (xmmsc_signal_visualisation_data (m_xmmsc)); }
XMMSResult *signal_mediainfo_reader_unindexed (void) { return new XMMSResult (xmmsc_signal_mediainfo_reader_unindexed (m_xmmsc)); }
XMMSResultDictList *medialib_select (const char * x) { return new XMMSResultDictList (xmmsc_medialib_select (m_xmmsc, x)); }
XMMSResult *medialib_playlist_save_current (const char * x) { return new XMMSResult (xmmsc_medialib_playlist_save_current (m_xmmsc, x)); }
XMMSResult *medialib_playlist_load (const char * x) { return new XMMSResult (xmmsc_medialib_playlist_load (m_xmmsc, x)); }
XMMSResult *medialib_add_entry (const char * x) { return new XMMSResult (xmmsc_medialib_add_entry (m_xmmsc, x)); }
XMMSResultDict *medialib_get_info (uint32_t x) { return new XMMSResultDict (xmmsc_medialib_get_info (m_xmmsc, x)); }
XMMSResult *medialib_add_to_playlist (const char * x) { return new XMMSResult (xmmsc_medialib_add_to_playlist (m_xmmsc, x)); }
XMMSResult *medialib_playlists_list (void) { return new XMMSResult (xmmsc_medialib_playlists_list (m_xmmsc)); }
XMMSResult *medialib_playlist_list (const char * x) { return new XMMSResult (xmmsc_medialib_playlist_list (m_xmmsc, x)); }
XMMSResult *medialib_playlist_import (const char * x,const char * y) { return new XMMSResult (xmmsc_medialib_playlist_import (m_xmmsc, x, y)); }
XMMSResult *medialib_playlist_export (const char * x,const char * y) { return new XMMSResult (xmmsc_medialib_playlist_export (m_xmmsc, x, y)); }
XMMSResult *medialib_playlist_remove (const char * x) { return new XMMSResult (xmmsc_medialib_playlist_remove (m_xmmsc, x)); }
XMMSResult *medialib_path_import (const char * x) { return new XMMSResult (xmmsc_medialib_path_import (m_xmmsc, x)); }
XMMSResult *medialib_rehash (uint32_t x) { return new XMMSResult (xmmsc_medialib_rehash (m_xmmsc, x)); }
XMMSResult *medialib_get_id (const char * x) { return new XMMSResult (xmmsc_medialib_get_id (m_xmmsc, x)); }
XMMSResult *medialib_remove_entry (int32_t x) { return new XMMSResult (xmmsc_medialib_remove_entry (m_xmmsc, x)); }
XMMSResult *medialib_entry_property_set (uint32_t x,char * y,char * z) { return new XMMSResult (xmmsc_medialib_entry_property_set (m_xmmsc, x, y, z)); }
XMMSResult *medialib_entry_property_set_with_source (uint32_t x,char * y,char * z,char * w) { return new XMMSResult (xmmsc_medialib_entry_property_set_with_source (m_xmmsc, x, y, z, w)); }
XMMSResultValue<uint> *broadcast_medialib_entry_changed (void) { return new XMMSResultValue<uint> (xmmsc_broadcast_medialib_entry_changed (m_xmmsc)); }
XMMSResult *broadcast_medialib_playlist_loaded (void) { return new XMMSResult (xmmsc_broadcast_medialib_playlist_loaded (m_xmmsc)); }
#endif
