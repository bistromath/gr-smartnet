#!/usr/bin/env python
""" Block to demodulate FSK via coherent PLL frequency estimation
	Not exactly hardcore but it works
"""


from gnuradio import gr, gru, blks2, digital
from gnuradio import eng_notation
from gnuradio.gr import firdes
from math import pi

def euclid(numA, numB):
	while numB != 0:
		numRem = numA % numB
		numA = numB
		numB = numRem
	return numA

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

		self._downsampletaps = gr.firdes.low_pass(1, self._samples_per_second, 10000, 1000, firdes.WIN_HANN)

		self._decim = int(self._samples_per_second / (self._syms_per_sec * self._clockrec_oversample))

		print "Demodulator decimation: %i" % (self._decim,)

		self._downsample = gr.freq_xlating_fir_filter_ccf(self._decim, #decimation
														  self._downsampletaps, #taps
														  self._freqoffset, #freq offset
														  self._samples_per_second) #sampling rate

		#these coeffs were found experimentally
		self._demod = gr.pll_freqdet_cf(0.8, #gain alpha, rad/samp
										6,  #max freq, rad/samp
										-6)  #min freq, rad/samp

		#this pll is low phase gain to track the carrier itself, to cancel out freq error
		#it's a continuous carrier so you don't have to worry too much about it locking fast
		self._carriertrack = gr.pll_freqdet_cf(10e-6,
											   6,
											   -6)

		self._lpfcoeffs = gr.firdes.low_pass(1, self._samples_per_second / self._decim, self._syms_per_sec, 100, firdes.WIN_HANN)

		self._lpf = gr.fir_filter_fff(self._filtdecim, #decimation
									  self._lpfcoeffs) #coeffs

		print "Samples per symbol: %f" % (float(self._samples_per_second)/self._decim/self._syms_per_sec,)
		self._softbits = digital.clock_recovery_mm_ff(float(self._samples_per_second)/self._decim/self._syms_per_sec,
												 0.25*self._gain_mu*self._gain_mu, #gain omega, = mu/2 * mu_gain^2
												 self._mu, #mu (decision threshold)
												 self._gain_mu, #mu gain
												 self._omega_relative_limit) #omega relative limit

		self._subtract = gr.sub_ff()

		self._slicer = digital.binary_slicer_fb()

		self.connect(self, self._downsample, self._demod, (self._subtract, 0))
		self.connect(self._downsample, self._carriertrack)
		self.connect(self._carriertrack, (self._subtract, 1))
		self.connect(self._subtract, self._lpf, self._softbits, self._slicer, self)
