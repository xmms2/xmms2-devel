# XMMS2 - X Music Multiplexer System
# Copyright (C) 2003-2014 XMMS2 Team
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.

# python -m unittest discover -s src/clients/nycli -p *.py -v

from collections import defaultdict
import subprocess
import re
import time
import unittest

class CommandResult(object):
    def __init__(self, stdout, stderr, success):
        self.stdout = stdout
        self.stderr = stderr
        self.success = success

    def __repr__(self):
        return "stdout: %s\nstderr: %s\ncode: %d" % (self.stdout, self.stderr, self.success)

def cmd(*args):
    proc = subprocess.Popen(("xmms2",) + tuple(str(x) for x in args), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    return CommandResult(stdout, stderr, proc.returncode == 0)

class Properties(object):
    def __init__(self, by_key, by_source):
        self.by_key = by_key
        self.by_source = by_source

    def __repr__(self):
        result = ""
        for s,kv in self.by_source.items():
            for k,v in kv.iteritems():
                result += "[%s] %s = %s\n" % (s, k, v)
        return result.strip()

    @staticmethod
    def parse(text):
        by_key = defaultdict(dict)
        by_source = defaultdict(dict)
        pattern = re.compile("\[([^]]+)\] ([^ ]+) = (.+)")
        for line in text.split("\n"):
            line = line.strip()
            if not line:
                continue
            source, key, value = pattern.match(line).groups()
            by_key[key][source] = value
            by_source[source][key] = value

        return Properties(by_key, by_source)

    @staticmethod
    def fetch(mid=None):
        if mid is not None:
            return Properties.parse(cmd("info", "#%d" % mid).stdout)
        return Properties.parse(cmd("info").stdout)

class PlaylistEntry(object):
    def __init__(self, mid, artist, title, duration):
        self.mid = mid
        self.artist = artist
        self.title = title
        self.duration = duration

        minutes, seconds = duration.split(":")
        self.seconds = int(minutes) * 60 + int(seconds)

    def __repr__(self):
        return "[%d] %s %s (%s)" % (self.mid, self.artist, self.title, self.duration)

class Playlist(object):
    def __init__(self, entries, current_position):
        self.entries = entries
        self.current_position = current_position

    def __repr__(self):
        result = ""
        for position, entry in enumerate(self.entries):
            result += str(position == self.current_position) + " " + str(entry) + "\n"
        return result.strip()

    @staticmethod
    def parse_columns(lines):
        pass

    @staticmethod
    def parse_classic(lines):
        entries = []
        start_position = 0
        end_position = 0
        current_position = 0
        pattern = re.compile("([^[]+)\[([^/]+)/([^]]+)\] (.+) - (.+) \(([^)]+)\)")
        for line in lines:
            if len(line) == 0 or line.startswith("Total"):
                continue
            status, position, mid, artist, title, duration = pattern.match(line).groups()
            if status == "->":
                current_position = int(position)
            if start_position == 0:
                start_position = position
            end_position = position
            entries.append(PlaylistEntry(int(mid), artist, title, duration))
        return Playlist(entries, current_position)

    @staticmethod
    def parse(text):
        lines = text.split("\n")
        if len(lines) > 0:
            if lines[0].startswith("--[Result]"):
                return Playlist.parse_columns(lines)
            return Playlist.parse_classic(lines)
        raise Exception("Could not parse playlist output")

    @staticmethod
    def fetch(playlist=None):
        if playlist is not None:
            return Playlist.parse(cmd("list", "-p", playlist).stdout)
        return Playlist.parse(cmd("list").stdout)

class Collections(object):
    def __init__(self, entries, active):
        self.entries = entries
        self.active = active

    @staticmethod
    def parse(text):
        active = None
        entries = []
        for line in text.split("\n"):
            if line.startswith("* "):
                active = line[2:]
            entries.append(line[2:])
        return Collections(entries, active)

    @staticmethod
    def fetch_playlists(playlist=None):
        if playlist is not None:
            return Collections.parse(cmd("playlist", "list", playlist).stdout)
        return Collections.parse(cmd("playlist", "list").stdout)

    @staticmethod
    def fetch_collections():
        return Collections.parse(cmd("collections", "list").stdout)

class KeyValue(object):
    @staticmethod
    def parse(text, func=None):
        kv = {}
        for line in text.split("\n"):
            line = line.strip()
            if not line:
                continue
            key, value = line.split("=")
            if func is not None:
                try:
                    kv[key.strip()] = func(value.strip())
                except:
                    print "Could not marshall '%s' = '%s'" % (key, value)
            else:
                kv[key.strip()] = value.strip()
        return kv

class Volume(object):
    @staticmethod
    def fetch():
        return KeyValue.parse(cmd("server", "volume").stdout, int)

class Config(object):
    @staticmethod
    def fetch():
        return KeyValue.parse(cmd("server", "config").stdout)

class BasicPlaylist(unittest.TestCase):
    TEST_PLAYLIST_1 = "cli-test-1"
    TEST_PLAYLIST_2 = "cli-test-2"
    TEST_PLAYLIST_3 = "cli-test-3"
    TEST_PLAYLIST_4 = "cli-test-4"

    def setUp(self):
        cmd("playlist", "create", BasicPlaylist.TEST_PLAYLIST_1)
        cmd("playlist", "clear", BasicPlaylist.TEST_PLAYLIST_1)
        cmd("add", "-p", BasicPlaylist.TEST_PLAYLIST_1, "album:\"Modern Day City Symphony\"", "AND", "url~Music")

        cmd("playlist", "create", BasicPlaylist.TEST_PLAYLIST_2)
        cmd("playlist", "clear", BasicPlaylist.TEST_PLAYLIST_2)
        cmd("add", "-p", BasicPlaylist.TEST_PLAYLIST_2, "album:\"Modern Day City Symphony\"", "AND", "url~Music")

        self.previous_playlist = Collections.fetch_playlists().active

        cmd("playlist", "switch", BasicPlaylist.TEST_PLAYLIST_1)

    def tearDown(self):
        cmd("playlist", "switch", self.previous_playlist)

        playlists = (
            BasicPlaylist.TEST_PLAYLIST_1,
            BasicPlaylist.TEST_PLAYLIST_2,
            BasicPlaylist.TEST_PLAYLIST_3,
            BasicPlaylist.TEST_PLAYLIST_4
        )
        for playlist in playlists:
            cmd("playlist", "remove", playlist)


class TestPlaylist(BasicPlaylist):
    def testListEntries(self):
        # list active playlist
        playlist = Playlist.parse(cmd("list").stdout)
        self.assertEqual(15, len(playlist.entries))

        # list specific playlist
        playlist = Playlist.parse(cmd("list", "-p", TestPlaylist.TEST_PLAYLIST_2).stdout)
        self.assertEqual(15, len(playlist.entries))

        # list pattern
        playlist = Playlist.parse(cmd("list", "-p", TestPlaylist.TEST_PLAYLIST_2, "the").stdout)
        self.assertEqual(4, len(playlist.entries))

        # list positions around current position
        cmd("jump", "5")
        playlist = Playlist.parse(cmd("list", "2_2").stdout)
        self.assertEqual(5, playlist.current_position)
        self.assertEqual(5, len(playlist.entries))

        # list a number of positions
        playlist = Playlist.parse(cmd("list", "2,4,6,8").stdout)
        self.assertEqual(4, len(playlist.entries))

    def testClear(self):
        # clear active playlist
        self.assertEqual(15, len(Playlist.fetch(TestPlaylist.TEST_PLAYLIST_1).entries))
        cmd("playlist", "clear")
        self.assertEqual(0, len(Playlist.fetch(TestPlaylist.TEST_PLAYLIST_1).entries))

        # clear specified playlist
        self.assertEqual(15, len(Playlist.fetch(TestPlaylist.TEST_PLAYLIST_2).entries))
        cmd("playlist", "clear", TestPlaylist.TEST_PLAYLIST_2)
        self.assertEqual(0, len(Playlist.fetch(TestPlaylist.TEST_PLAYLIST_2).entries))

    def testRename(self):
        # rename active playlist
        cmd("playlist", "rename", TestPlaylist.TEST_PLAYLIST_3)
        self.assertTrue(TestPlaylist.TEST_PLAYLIST_3 in Collections.fetch_playlists().entries)

        # rename specified playlist
        cmd("playlist", "rename", "-p", TestPlaylist.TEST_PLAYLIST_2, TestPlaylist.TEST_PLAYLIST_4)
        self.assertTrue(TestPlaylist.TEST_PLAYLIST_4 in Collections.fetch_playlists().entries)

    def testCreate(self):
        # create with collision
        self.assertTrue("Error" in cmd("playlist", "create", TestPlaylist.TEST_PLAYLIST_1).stdout)

        # create new empty playlist
        self.assertTrue(cmd("playlist", "create", TestPlaylist.TEST_PLAYLIST_3).stdout.strip() == "")
        self.assertTrue("cli-test-3" in Collections.fetch_playlists().entries)
        self.assertEqual(0, len(Playlist.fetch(TestPlaylist.TEST_PLAYLIST_3).entries))

        # create new playlist and copy contents from another playlist
        self.assertTrue(cmd("playlist", "create", "-p", TestPlaylist.TEST_PLAYLIST_1, TestPlaylist.TEST_PLAYLIST_4).stdout.strip() == "")
        self.assertTrue("cli-test-3" in Collections.fetch_playlists().entries)
        self.assertEqual(15, len(Playlist.fetch(TestPlaylist.TEST_PLAYLIST_4).entries))

    def testShuffle(self):
        # shuffle active playlist
        before = list((x.artist, x.title) for x in Playlist.fetch().entries)
        cmd("playlist", "shuffle")
        after = list((x.artist, x.title) for x in Playlist.fetch().entries)
        self.assertNotEqual(before, after)
        self.assertEqual(set(before), set(after))

        # shuffle specific playlist
        before = list((x.artist, x.title) for x in Playlist.fetch(TestPlaylist.TEST_PLAYLIST_2).entries)
        cmd("playlist", "shuffle", TestPlaylist.TEST_PLAYLIST_2)
        after = list((x.artist, x.title) for x in Playlist.fetch(TestPlaylist.TEST_PLAYLIST_2).entries)
        self.assertNotEqual(before, after)
        self.assertEqual(set(before), set(after))

    def testSort(self):
        # sort active playlist by duration
        before = list(x.seconds for x in Playlist.fetch().entries)
        cmd("playlist", "sort", "duration")
        after = list(x.seconds for x in Playlist.fetch().entries)
        self.assertNotEqual(before, after)
        before.sort()
        self.assertEqual(before, after)

        # sort specific playlist by duration
        before = list(x.seconds for x in Playlist.fetch(TestPlaylist.TEST_PLAYLIST_2).entries)
        cmd("playlist", "sort", "-p", TestPlaylist.TEST_PLAYLIST_2, "duration")
        after = list(x.seconds for x in Playlist.fetch(TestPlaylist.TEST_PLAYLIST_2).entries)
        self.assertNotEqual(before, after)
        before.sort()
        self.assertEqual(before, after)

    def testClear(self):
        before = Playlist.fetch().entries
        cmd("playlist", "clear")
        after = Playlist.fetch().entries
        self.assertNotEqual(len(before), len(after))
        self.assertEqual(0, len(after))

        before = Playlist.fetch(TestPlaylist.TEST_PLAYLIST_2).entries
        cmd("playlist", "clear", TestPlaylist.TEST_PLAYLIST_2)
        after = Playlist.fetch(TestPlaylist.TEST_PLAYLIST_2).entries
        self.assertNotEqual(len(before), len(after))
        self.assertEqual(0, len(after))

    def testSwitch(self):
        # switch to second test playlist
        self.assertEqual(TestPlaylist.TEST_PLAYLIST_1, Collections.fetch_playlists().active)
        cmd("playlist", "switch", TestPlaylist.TEST_PLAYLIST_2)
        self.assertEqual(TestPlaylist.TEST_PLAYLIST_2, Collections.fetch_playlists().active)
        # and switch back again
        cmd("playlist", "switch", TestPlaylist.TEST_PLAYLIST_1)
        self.assertEqual(TestPlaylist.TEST_PLAYLIST_1, Collections.fetch_playlists().active)

    def testList(self):
        # list playlists
        result = cmd("playlist", "list")
        self.assertTrue(TestPlaylist.TEST_PLAYLIST_1 in result.stdout)
        self.assertTrue(TestPlaylist.TEST_PLAYLIST_2 in result.stdout)

        # list playlists + hidden playlists
        result = cmd("playlist", "list", "-a")
        self.assertTrue(TestPlaylist.TEST_PLAYLIST_1 in result.stdout)
        self.assertTrue(TestPlaylist.TEST_PLAYLIST_2 in result.stdout)
        self.assertTrue("_active" in result.stdout)

class TestServer(BasicPlaylist):
    def testBrowse(self):
        self.assertTrue("file://" in cmd("server", "browse", "file:///").stdout)

    def testPlugins(self):
        self.assertTrue("segment" in cmd("server", "plugins").stdout)

    def testVolume(self):
        original = Volume.fetch()

        cmd("server", "volume", "100")
        volume = Volume.fetch()
        self.assertEqual(set([100]), set(volume.values()))

        cmd("server", "volume", "0")
        volume = Volume.fetch()
        self.assertEqual(set([0]), set(volume.values()))

        cmd("server", "volume", "+10")
        volume = Volume.fetch()
        self.assertEqual(set([10]), set(volume.values()))

        cmd("server", "volume", "-5")
        volume = Volume.fetch()
        self.assertEqual(set([5]), set(volume.values()))

        volume = Volume.fetch()
        channel = volume.iterkeys().next()
        cmd("server", "volume", "-c", channel, "42")
        volume = Volume.fetch()
        self.assertTrue(42 in set(volume.values()))

        for channel, volume in original.iteritems():
            cmd("server", "volume", "-c", channel, volume)

    def testSync(self):
        self.assertTrue("Error" not in cmd("server", "sync").stdout)

    def testStats(self):
        result = cmd("server", "stats")
        self.assertTrue("uptime" in result.stdout)
        self.assertTrue("version" in result.stdout)

    def testRehash(self):
        entry = iter(Playlist.fetch().entries).next()

        before = Properties.fetch(entry.mid)
        for source in before.by_key["artist"].iterkeys():
            cmd("server", "property", "-S", source, "-D", entry.mid, "artist")

        after = Properties.fetch(entry.mid)
        self.assertNotEqual(before.by_key["artist"], after.by_key["artist"])
        cmd("server", "rehash", "#%d" % entry.mid)

        # TODO: replace with await(lambda) as rehash is async
        time.sleep(0.5)

        after = Properties.fetch(entry.mid)
        self.assertEqual(before.by_key["artist"], after.by_key["artist"])

    def testConfig(self):
        before = Config.fetch()
        self.assertTrue("output.buffersize" in before.keys())

        # change config setting
        cmd("server", "config", "output.buffersize", 512)
        after = Config.fetch()
        self.assertNotEqual(before["output.buffersize"], after["output.buffersize"])

        # restore config setting
        cmd("server", "config", "output.buffersize", before["output.buffersize"])
        after = Config.fetch()
        self.assertEqual(before["output.buffersize"], after["output.buffersize"])

        # print single
        result = cmd("server", "config", "output.buffersize")
        self.assertTrue("output.buffersize" in result.stdout)

        # print match
        result = cmd("server", "config", "*buffer*")
        self.assertTrue("output.buffersize" in result.stdout)


class TestPlayback(BasicPlaylist):
    def testPlaybackToggle(self):
        self.assertTrue(cmd("stop").success)
        self.assertEquals(cmd("current", "-f", "${playback_status}").stdout.strip(), "Stopped")
        self.assertTrue(cmd("play").success)
        self.assertEquals(cmd("current", "-f", "${playback_status}").stdout.strip(), "Playing")
        self.assertTrue(cmd("stop").success)

    def testPlaybackSeek(self):
        def pos():
            minutes, seconds = cmd("current", "-f", "${playtime}").stdout.strip().split(":")
            return int(minutes) * 60 + int(seconds)

        self.assertTrue(cmd("stop").success)
        self.assertTrue(cmd("play").success)
        # TODO: Use await() here
        time.sleep(0.2)
        self.assertEquals(0, pos())
        self.assertTrue(cmd("seek", "20").success)
        self.assertEquals(20, pos())
        self.assertTrue(cmd("seek", "0").success)
        # TODO: Use await() here
        time.sleep(0.2)
        self.assertTrue(pos() < 20)

if __name__ == '__main__':
    unittest.main()
