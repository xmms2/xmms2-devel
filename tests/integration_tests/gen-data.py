#!/usr/bin/env python
import sys
import wave
import struct
import math
from itertools import count, repeat
from cStringIO import StringIO
from contextlib import closing
from subprocess import call

CHANNELS = 1
FORMAT = 16
SAMPLERATE = 44100
DURATION = 10

notes = {
    "C4": 261.63,
    "D4": 293.66,
    "E4": 329.63,
    "F4": 349.23,
    "G4": 392.00,
    "A4": 440.00,
    "B4": 493.88
}

def sine_wave(frequency=440.0, samplerate=44100, amplitude=0.5, repeat=0):
    """
    Slightly modified version of : http://zacharydenton.com/generate-audio-with-python/
    """
    period = int(samplerate / frequency)
    if amplitude > 1.0: amplitude = 1.0
    if amplitude < 0.0: amplitude = 0.0
    lookup_table = [float(amplitude) * math.sin(2.0*math.pi*float(frequency)*(float(i%period)/float(samplerate))) for i in xrange(period)]
    return (lookup_table[i%period]*samplerate for i in xrange(repeat))

def write(filename, freq):
    print "* Writing %(duration)ds of %(freq).2fHz to %(filename)s" % { "duration": DURATION, "freq": freq, "filename": filename }
    with closing(wave.open(filename, "wb")) as fd:
        fd.setparams((CHANNELS, FORMAT / 8, SAMPLERATE, SAMPLERATE * DURATION, 'NONE', 'noncompressed'))
        buffer = StringIO()
        for x in sine_wave(freq, repeat=SAMPLERATE * DURATION):
            buffer.write(struct.pack("h" * CHANNELS, *([x] * CHANNELS)))
        fd.writeframes(buffer.getvalue())

def ffmpeg(name, params, input, output, tags={}):
    print "* Encoding %(name)s from %(input)s to %(output)s" % { "name": name, "input": input, "output": output }
    metadata = []
    for tag, value in tags.items():
        metadata += ["-metadata", tag + "=" + value]
    if call(["ffmpeg", "-y", "-loglevel", "error", "-i", input] + params + metadata + [output]) != 0:
        print "x FAILED!"

write("test.wav", 440)

metadata = { "artist": "Professor Hertz", "title": "Aaaa yeah" }

ffmpeg("MP4+AAC", ["-ac", "2", "-c:a", "libfaac"],
       "test.wav", "test.m4a", metadata)
ffmpeg("WavPack", ["-ac", "2", "-c:a", "wavpack"],
       "test.wav", "test.wv", metadata)
ffmpeg("AC-3", ["-ac", "2", "-c:a", "ac3"],
       "test.wav", "test.ac3", metadata)
ffmpeg("DTS", ["-ac", "2", "-strict", "experimental", "-c:a", "dts"],
       "test.wav", "test.dts", metadata)
ffmpeg("FLAC", ["-ac", "2", "-c:a", "flac"],
       "test.wav", "test.flac", metadata)
ffmpeg("MP3", ["-ac", "2", "-c:a", "mp3"],
       "test.wav", "test.mp3", metadata)
ffmpeg("FLV+NellyMoser", ["-ac", "1", "-c:a", "nellymoser"],
       "test.wav", "test+nellymoser.flv", metadata)
ffmpeg("FLV+AAC", ["-ac", "2", "-c:a", "libfaac"],
       "test.wav", "test+aac.flv", metadata)
ffmpeg("FLV+MP3", ["-ac", "2", "-c:a", "mp3"],
       "test.wav", "test+mp3.flv", metadata)
ffmpeg("FLV+PCM", ["-ac", "2", "-c:a", "pcm_s16le"],
       "test.wav", "test+pcm.flv", metadata)
ffmpeg("FLV+SWF", ["-ac", "2", "-c:a", "adpcm_swf"],
       "test.wav", "test+swf.flv", metadata)
ffmpeg("Ogg+Opus", ["-ac", "2", "-c:a", "opus"],
       "test.wav", "test.opus", metadata)
ffmpeg("Ogg+Vorbis", ["-ac", "2", "-c:a", "libvorbis"],
       "test.wav", "test.ogg", metadata)
ffmpeg("Ogg+Speex", ["-ac", "2", "-c:a", "libspeex"],
       "test.wav", "test.spx", metadata)
ffmpeg("WMA v1", ["-ac", "1", "-c:a", "wmav1"],
       "test.wav", "test+wmav1.wma", metadata)
ffmpeg("WMA v2", ["-ac", "1", "-c:a", "wmav2"],
       "test.wav", "test+wmav2.wma", metadata)
ffmpeg("AIFF", ["-ac", "2", "-c:a", "pcm_s16be"],
       "test.wav", "test.aiff", metadata)
ffmpeg("CAF", ["-ac", "2", "-c:a", "pcm_s16be"],
       "test.wav", "test.caf", metadata)
ffmpeg("AU", ["-ac", "2", "-c:a", "pcm_s16be"],
       "test.wav", "test.au", metadata)

# No TTA muxer in ffmpeg at this point in time.
#ffmpeg("TTA", ["-f", "rawvideo", "-ac", "2", "-c:a", "ttaenc"],
#       "test.wav", "test.tta", metadata)
