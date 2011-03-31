/* -*- c++ -*- */
/*
 * Copyright 2004,2006,2007,2008,2009 Free Software Foundation, Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <smartnet_wavsink.h>
#include <gr_io_signature.h>
#include <gri_wavfile.h>
#include <stdexcept>
#include <climits>
#include <cstring>
#include <cmath>
#include <fcntl.h>
#include <gruel/thread.h>
#include <sys/stat.h>

// win32 (mingw/msvc) specific
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef O_BINARY
#define OUR_O_BINARY O_BINARY
#else
#define OUR_O_BINARY 0
#endif

// should be handled via configure
#ifdef O_LARGEFILE
#define OUR_O_LARGEFILE O_LARGEFILE
#else
#define OUR_O_LARGEFILE 0
#endif


smartnet_wavsink_sptr
smartnet_make_wavsink(const char *filename,
		     int n_channels,
		     unsigned int sample_rate,
		     int bits_per_sample)
{
  return smartnet_wavsink_sptr (new smartnet_wavsink (filename,
						    n_channels,
						    sample_rate,
						    bits_per_sample));
}

smartnet_wavsink::smartnet_wavsink(const char *filename,
				 int n_channels,
				 unsigned int sample_rate,
				 int bits_per_sample)
  : gr_sync_block ("wavsink",
		   gr_make_io_signature(1, n_channels, sizeof(float)),
		   gr_make_io_signature(0, 0, 0)),
    d_sample_rate(sample_rate), d_nchans(n_channels),
    d_fp(0), d_new_fp(0), d_updated(false)
{
  if (bits_per_sample != 8 && bits_per_sample != 16) {
    throw std::runtime_error("Invalid bits per sample (supports 8 and 16)");
  }
  d_bytes_per_sample = bits_per_sample / 8;
  d_bytes_per_sample_new = d_bytes_per_sample;
  
  if (!open(filename)) {
    throw std::runtime_error ("can't open file");
  }

  if (bits_per_sample == 8) {
    d_max_sample_val = 0xFF;
    d_min_sample_val = 0;
    d_normalize_fac  = d_max_sample_val/2;
    d_normalize_shift = 1;
  } else {
    d_max_sample_val = 0x7FFF;
    d_min_sample_val = -0x7FFF;
    d_normalize_fac  = d_max_sample_val;
    d_normalize_shift = 0;
    if (bits_per_sample != 16) {
      fprintf(stderr, "Invalid bits per sample value requested, using 16");
    }
  }
}


bool
smartnet_wavsink::open(const char* filename)
{
	//this function is modified to append to .wav files instead of overwriting.
  int fd;
  gruel::scoped_lock guard(d_mutex);

	//first, check to see if the file exists.
	int filestat = 0;
	struct stat fileinfo;

	filestat = stat(filename, &fileinfo);

	if(filestat == 0 && fileinfo.st_size != 0) { //the file exists and is nonzero in size, so we must append to the .wav
	//open() and close() are both thread-safe public functions. they let you close and open files, so that's already in place.
	//all we need to add is support for appending to a file rather than overwriting it.

	//all right, here's the way this is gonna work. open the file just like gr_wavfile_source does...
		if((fd = ::open (filename,O_RDWR|OUR_O_LARGEFILE|OUR_O_BINARY)) < 0) { //we don't use O_APPEND because we need to overwrite the header
			perror(filename);
			return false;
		}
		if (d_new_fp) {    // if we've already got a new one open, close it
			//printf("New fp found, closing\n");
	    fclose(d_new_fp);
	    d_new_fp = 0;
	  }

  	if ((d_new_fp = fdopen (fd, "r+b")) == NULL) {
  	  perror (filename);
  	  ::close(fd);  // don't leak file descriptor if fdopen fails.
  	  return false;
  	}
	 	d_updated = true;
		unsigned new_sample_rate;
		int new_nchans, new_bytes_per_sample, new_first_sample_pos;

		//validate the data, be sure it's set up the same as the current block (sample rate, mono/stereo, etc) and barf if it isn't
	  if (!gri_wavheader_parse(d_new_fp,
				   new_sample_rate,
				   new_nchans,
				   new_bytes_per_sample,
				   new_first_sample_pos,
				   d_new_samples_per_chan)) {
	    throw std::runtime_error("is not a valid wav file");
	  }

		//printf("Appended file %s has sample count %u and filesize %u\n", filename, d_new_samples_per_chan, fileinfo.st_size);
		//printf("Appended file %s has %f seconds of audio\n", filename, (float(d_new_samples_per_chan)/new_sample_rate));

		if((new_sample_rate != d_sample_rate) || (new_nchans != d_nchans) || (new_bytes_per_sample != d_bytes_per_sample)) {
			throw std::runtime_error("WAV file with incompatible sample rate or channels");
		}

		d_new_append = true;
		fseek(d_new_fp, 0, SEEK_END); //seek to the end of the file and start writin'

	}	else {
	  // we use the open system call to get access to the O_LARGEFILE flag.
	  if ((fd = ::open (filename,
			    O_WRONLY|O_CREAT|O_TRUNC|OUR_O_LARGEFILE|OUR_O_BINARY,
			    0664)) < 0){
	    perror (filename);
	    return false;
	  }
		d_new_append = false;

		if (d_new_fp) {    // if we've already got a new one open, close it
	    fclose(d_new_fp);
	    d_new_fp = 0;
	  }
  	if ((d_new_fp = fdopen (fd, "wb")) == NULL) {
  	  perror (filename);
  	  ::close(fd);  // don't leak file descriptor if fdopen fails.
  	  return false;
  	}
	 	d_updated = true;

	  if (!gri_wavheader_write(d_new_fp,
				   d_sample_rate,
				   d_nchans,
				   d_bytes_per_sample_new)) {
	    fprintf(stderr, "[%s] could not write to WAV file\n", __FILE__);
	    exit(-1);
	  }

	}
  
  return true;
}


void
smartnet_wavsink::close()
{
  gruel::scoped_lock guard(d_mutex);
  
  if (!d_fp)
    return;
  
  close_wav();
}

void smartnet_wavsink::close_wav()
{
  unsigned int byte_count = d_sample_count * d_bytes_per_sample;

	//printf("Writing wav header with %f seconds of audio\n", ((float(d_sample_count)/d_sample_rate)/d_nchans)/d_bytes_per_sample);
  
  if(!gri_wavheader_complete(d_fp, byte_count)) {
		throw std::runtime_error("Error writing wav header\n");
	}
  
	fclose(d_fp);
  d_fp = NULL;
}


smartnet_wavsink::~smartnet_wavsink ()
{
  if (d_new_fp) {
    fclose(d_new_fp);
  }

  close();
}


int
smartnet_wavsink::work (int noutput_items,
		       gr_vector_const_void_star &input_items,
		       gr_vector_void_star &output_items)
{
  float **in = (float **) &input_items[0];
  int n_in_chans = input_items.size();
  
  short int sample_buf_s;
  
  int nwritten;
  
  do_update();	// update: d_fp is reqd
  if (!d_fp)	// drop output on the floor if there isn't a valid file open
    return noutput_items;
  
  for (nwritten = 0; nwritten < noutput_items; nwritten++) {
    for (int chan = 0; chan < d_nchans; chan++) {
      // Write zeros to channels which are in the WAV file
      // but don't have any inputs here
      if (chan < n_in_chans) {
				sample_buf_s = convert_to_short(in[chan][nwritten]);
      } else {
				sample_buf_s = 0;
      }
      
      gri_wav_write_sample(d_fp, sample_buf_s, d_bytes_per_sample);
      
      if (feof(d_fp) || ferror(d_fp)) {
				fprintf(stderr, "[%s] file i/o error\n", __FILE__);
				close();
				exit(-1);
      }
      d_sample_count++;
    }
  }
  
  return nwritten;
}


short int
smartnet_wavsink::convert_to_short(float sample)
{
  sample += d_normalize_shift;
  sample *= d_normalize_fac;
  if (sample > d_max_sample_val) {
    sample = d_max_sample_val;
  } else if (sample < d_min_sample_val) {
    sample = d_min_sample_val;
  }
  
  return (short int) roundf(sample);
}


void
smartnet_wavsink::set_bits_per_sample(int bits_per_sample)
{
  gruel::scoped_lock guard(d_mutex);
  if (bits_per_sample == 8 || bits_per_sample == 16) {
    d_bytes_per_sample_new = bits_per_sample / 8;
  }
}


void
smartnet_wavsink::set_sample_rate(unsigned int sample_rate)
{
  gruel::scoped_lock guard(d_mutex);
  d_sample_rate = sample_rate;
}


void
smartnet_wavsink::do_update()
{
  if (!d_updated) {
    return;
  }
  
  gruel::scoped_lock guard(d_mutex);     // hold mutex for duration of this block
  if (d_fp) {
    close_wav();
  }

  d_fp = d_new_fp;                    // install new file pointer
  d_new_fp  = 0;
	if (d_new_append) {
		d_sample_count = d_new_samples_per_chan * d_nchans; //set the sample count
	} else {
	  d_sample_count = 0;
	}
  d_bytes_per_sample = d_bytes_per_sample_new;

  if (d_bytes_per_sample == 1) {
    d_max_sample_val = UCHAR_MAX;
    d_min_sample_val = 0;
    d_normalize_fac  = d_max_sample_val/2;
    d_normalize_shift = 1;
  } else if (d_bytes_per_sample == 2) {
    d_max_sample_val = SHRT_MAX;
    d_min_sample_val = SHRT_MIN;
    d_normalize_fac  = d_max_sample_val;
    d_normalize_shift = 0;
  }
  
  d_updated = false;
}
