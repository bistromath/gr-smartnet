#!/usr/bin/env python
""" Block to demodulate FSK via coherent PLL frequency estimation
	Not exactly hardcore but it works
"""


from gnuradio import gr, gru, blks2, digital, window
from gnuradio import eng_notation
from gnuradio.gr import firdes
import gr_ais
from math import pi

class fsk_demod(gr.hier_block2):
	def __init__(self, options):
		gr.hier_block2.__init__(self, "fsk_demod",
                                gr.io_signature(1, 1, gr.sizeof_gr_complex), # Input signature
                                gr.io_signature(1, 1, gr.sizeof_char)) # Output signature

		self._syms_per_sec = options.syms_per_sec # ditto
		self._samples_per_second = options.samples_per_second
		self._gain_mu = options.gain_mu # for the clock recovery block
		self._mu = options.mu
		self._omega_relative_limit = options.omega_relative_limit

		self._freqoffset = options.offset

		self._filtdecim = 1

		#first bring that input stream down to a manageable level, let's say 10 samples per bit. that's 36kS/s.
		self._clockrec_oversample = 10

		#these taps are for a channel selection filter
		self._downsampletaps = gr.firdes.low_pass(1, self._samples_per_second, 10000, 1000, firdes.WIN_HANN)

		self._decim = int(self._samples_per_second / (self._syms_per_sec * self._clockrec_oversample))

		print "Demodulator decimation: %i" % (self._decim,)

		self._downsample = gr.freq_xlating_fir_filter_ccf(self._decim, #decimation
														  self._downsampletaps, #taps
														  self._freqoffset, #freq offset
														  self._samples_per_second) #sampling rate

		#using a pll to demod gets you a nice IIR LPF response for free
		self._demod = gr.pll_freqdet_cf(0.20, #gain alpha, rad/samp, experimentally determined
										 2*pi/self._clockrec_oversample,  #max freq, rad/samp
										-2*pi/self._clockrec_oversample)  #min freq, rad/samp

		self._sps = float(self._samples_per_second)/self._decim/self._syms_per_sec

		#band edge filter FLL with a low bandwidth is very good
		#at synchronizing to continuous FSK signals
		self._carriertrack = digital.fll_band_edge_cc(self._sps,
													  0.6, #rolloff factor
													  64,  #taps
													  1.0) #loop bandwidth

		print "Samples per symbol: %f" % (self._sps,)
		self._softbits = digital.clock_recovery_mm_ff(self._sps,
												 0.25*self._gain_mu*self._gain_mu, #gain omega, = mu/2 * mu_gain^2
												 self._mu, #mu (decision threshold)
												 self._gain_mu, #mu gain
												 self._omega_relative_limit) #omega relative limit

		self._subtract = gr.sub_ff()

		self._slicer = digital.binary_slicer_fb()

		self.connect(self, self._downsample, self._carriertrack, self._demod, self._softbits, self._slicer, self)
