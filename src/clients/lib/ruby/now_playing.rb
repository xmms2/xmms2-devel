#!/usr/bin/ruby

$MODE = "GLIB"
$MODE = "ECORE" if ARGV[0] == "-ecore"

require "xmmsclient"
require "ecore" if $MODE == "ECORE"
require "gtk2" if $MODE == "GLIB"

Gtk.init if $MODE == "GLIB"

xc = XmmsClient::XmmsClient.new

xc.connect("Ruby")

xc.setup_with_ecore if $MODE == "ECORE"
xc.setup_with_gmain if $MODE == "GLIB"

xc.broadcast_playback_current_id.notifier do |r|
	puts "now playing: #{r.uint}"
	r.restart
end

Ecore::main_loop_begin if $MODE == "ECORE"
Gtk.main if $MODE == "GLIB"
