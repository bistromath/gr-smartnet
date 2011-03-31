/* -*- c++ -*- */
/*
 * Copyright 2008,2009 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifndef INCLUDED_smartnet_wavsink_H
#define INCLUDED_smartnet_wavsink_H

#include <gr_sync_block.h>
#include <gr_file_sink_base.h>
#include <boost/thread.hpp>

class smartnet_wavsink;
typedef boost::shared_ptr<smartnet_wavsink> smartnet_wavsink_sptr;

/*
 * \p filename The .wav file to be opened
 * \p n_channels Number of channels (2 = stereo or I/Q output)
 * \p sample_rate Sample rate [S/s]
 * \p bits_per_sample 16 or 8 bit, default is 16
 */
smartnet_wavsink_sptr
smartnet_make_wavsink (const char *filename,
		      int n_channels,
		      unsigned int sample_rate,
		      int bits_per_sample = 16);

/*!
 * \brief Read stream from a Microsoft PCM (.wav) file, output floats
 *
 * Values are within [-1;1].
 * Check gr_make_wavfile_source() for extra info.
 *
 * \ingroup sink_blk
 */
class smartnet_wavsink : public gr_sync_block
{
private:
  friend smartnet_wavsink_sptr smartnet_make_wavsink (const char *filename,
						    int n_channels,
						    unsigned int sample_rate,
						    int bits_per_sample);

  smartnet_wavsink(const char *filename,
		  int n_channels,
		  unsigned int sample_rate,
		  int bits_per_sample);

  unsigned d_sample_rate;
  int d_nchans;
  unsigned d_sample_count;
  int d_bytes_per_sample;
  int d_bytes_per_sample_new;
  int d_max_sample_val;
  int d_min_sample_val;
  int d_normalize_shift;
  int d_normalize_fac;

	unsigned d_new_samples_per_chan;

  FILE *d_fp;
  FILE *d_new_fp;
  bool d_updated;
	bool d_new_append;
  boost::mutex d_mutex;
  
  /*!
   * \brief Convert a sample value within [-1;+1] to a corresponding
   *  short integer value
   */
  short convert_to_short(float sample);

  /*!
   * \brief Writes information to the WAV header which is not available
   * a-priori (chunk size etc.) and closes the file. Not thread-safe and
   * assumes d_fp is a valid file pointer, should thus only be called by
   * other methods.
   */
  void close_wav();

public:
  ~smartnet_wavsink ();

  /*!
   * \brief Opens a new file and writes a WAV header. Thread-safe.
   */
  bool open(const char* filename);

  /*!
   * \brief Closes the currently active file and completes the WAV
   * header. Thread-safe.
   */
  void close();

  /*!
   * \brief If any file changes have occurred, update now. This is called
   * internally by work() and thus doesn't usually need to be called by
   * hand.
   */
  void do_update();
  
  /*!
   * \brief Set the sample rate. This will not affect the WAV file
   * currently opened. Any following open() calls will use this new
   * sample rate.
   */
  void set_sample_rate(unsigned int sample_rate);
  
  /*!
   * \brief Set bits per sample. This will not affect the WAV file
   * currently opened (see set_sample_rate()). If the value is neither
   * 8 nor 16, the call is ignored and the current value is kept.
   */
  void set_bits_per_sample(int bits_per_sample);

	//returns the current time offset of the .wav file, for timestamp purposes
	float get_time(void) { 
		if(d_fp) { 
			return (float(d_sample_count)/d_sample_rate)/d_nchans;
		} else if(d_new_fp && d_new_append) {
			return float(d_new_samples_per_chan)/d_sample_rate;
		} else return 0;
  }
  
  int work(int noutput_items,
	   gr_vector_const_void_star &input_items,
	   gr_vector_void_star &output_items);
  
};

#endif /* INCLUDED_smartnet_wavsink_H */
