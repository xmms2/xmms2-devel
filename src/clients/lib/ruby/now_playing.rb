#!/usr/bin/ruby

$MODE = "GLIB"
$MODE = "ECORE" if ARGV[0] == "-ecore"

require "xmmsclient"

if $MODE == "ECORE"
	require "xmmsclient_ecore"
else
	require "gtk2"
	require "xmmsclient_glib"
end

Gtk.init if $MODE == "GLIB"

xc = XmmsClient::XmmsClient.new("Ruby")

xc.connect

xc.add_to_ecore_mainloop if $MODE == "ECORE"
xc.add_to_glib_mainloop if $MODE == "GLIB"

xc.broadcast_playback_current_id.notifier do |r|
	puts "now playing: #{r.value}"
end

Ecore::main_loop_begin if $MODE == "ECORE"
Gtk.main if $MODE == "GLIB"
