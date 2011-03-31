#!/usr/env/python

from gnuradio import blks2, gr, gru
from gnuradio.gr import firdes
from grc_gnuradio import blks2 as grc_blks2
from gnuradio import smartnet

import string
import random
import time, datetime
import os

class logging_receiver(gr.hier_block2):
	def __init__(self, talkgroup, options):
		gr.hier_block2.__init__(self, "fsk_demod",
                                gr.io_signature(1, 1, gr.sizeof_gr_complex), # Input signature
                                gr.io_signature(0, 0, gr.sizeof_char)) # Output signature

		#print "Starting log_receiver init()"

		self.audiorate = options.audiorate
		self.rate = options.rate
		self.talkgroup = talkgroup
		self.directory = options.directory

		if options.squelch is None:
			options.squelch = 28

		if options.volume is None:
			options.volume = 3.0

		self.audiotaps = gr.firdes.low_pass(1, self.rate, 8000, 2000, firdes.WIN_HANN)

		self.prefilter_decim = (self.rate / self.audiorate)

		#the audio prefilter is a channel selection filter.
		self.audio_prefilter = gr.freq_xlating_fir_filter_ccc(self.prefilter_decim, #decimation
								      self.audiotaps, #taps
								      0, #freq offset
								      self.rate) #sampling rate

		#on a trunked network where you know you will have good signal, a carrier power squelch works well. real FM receviers use a noise squelch, where
		#the received audio is high-passed above the cutoff and then fed to a reverse squelch. If the power is then BELOW a threshold, open the squelch.
		self.squelch = gr.pwr_squelch_cc(options.squelch, #squelch point
										   alpha = 0.1, #wat
										   ramp = 10, #wat
										   gate = True) #gated so that the audio recording doesn't contain blank spaces between transmissions

		self.audiodemod = blks2.fm_demod_cf(self.rate/self.prefilter_decim, #rate
						    1, #audio decimation
						    4000, #deviation
						    3000, #audio passband
						    4000, #audio stopband
						    options.volume, #gain
						    75e-6) #deemphasis constant

		#the filtering removes FSK data woobling from the subaudible channel
		self.audiofilttaps = gr.firdes.high_pass(1, self.audiorate, 300, 50, firdes.WIN_HANN)

		self.audiofilt = gr.fir_filter_fff(1, self.audiofilttaps)
		
		#self.audiogain = gr.multiply_const_ff(options.volume)

		#here we generate a random filename in the form /tmp/[random].wav, and then use it for the wavstamp block. this avoids collisions later on. remember to clean up these files when deallocating.

		self.tmpfilename = "/tmp/%s.wav" % ("".join([random.choice(string.letters+string.digits) for x in range(8)])) #if this looks glaringly different, it's because i totally cribbed it from a blog.

		self.valve = grc_blks2.valve(gr.sizeof_float, bool(1))

		#self.prefiltervalve = grc_blks2.valve(gr.sizeof_gr_complex, bool(1))

		#open the logfile for appending
		self.timestampfilename = "%s/%i.txt" % (self.directory, self.talkgroup)
		self.timestampfile = open(self.timestampfilename, 'a');

		self.filename = "%s/%i.wav" % (self.directory, self.talkgroup)
		self.audiosink = smartnet.wavsink(self.filename, 1, self.audiorate, 8) #this version allows appending to existing files.

		self.connect(self, self.audio_prefilter, self.squelch, self.audiodemod, self.valve, self.audiofilt, self.audiosink)

		self.timestamp = 0.0

		#print "Finishing logging receiver init()."

		self.mute() #start off muted.

	def __del__(self):
		#self.close()
		#self.audiosink.close()
		#os.system("rm %s" % self.tmpfilename) #remove the temp file you used for wav stamping
		self.timestampfile.close()

	def tuneoffset(self, target_freq, rffreq):
		self.audio_prefilter.set_center_freq(rffreq-target_freq*1e6)
		self.freq = target_freq

	def getfreq(self, rffreq):
		return self.freq

	def close(self): #close out and quit!
		self.mute() #make sure you aren't going to be writing
		self.audiosink.close() #if you write after this it's going to throw all the errors


	def mute(self):
		self.valve.set_open(bool(1))
#		self.prefiltervalve.set_open(bool(1))

	def unmute(self):
		self.valve.set_open(bool(0))
#		self.prefiltervalve.set_open(bool(0))
		if (self.timeout()) >= 3:
			self.stamp()

		self.timestamp = time.time()

	def timeout(self):
		return time.time() - self.timestamp

	def stamp(self):
		#print "Stamp says the current wavtime is %f" % self.audiosink.get_time()
		current_wavtime = self.audiosink.get_time() #gets the time in fractional seconds corresponding to the current position in the audio file
		current_timestring = time.strftime("%m/%d/%y %H:%M:%S")
		current_timestampstring = str(datetime.timedelta(seconds=current_wavtime)) + ": " + current_timestring + "\n"
		self.timestampfile.write(current_timestampstring)
		self.timestampfile.flush() #so you can follow along

