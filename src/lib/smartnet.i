/*
 * Copyright 2007 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
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

%feature("autodoc","1");
%include "exception.i"
%import "gnuradio.i"

%{
#include "gnuradio_swig_bug_workaround.h"	// mandatory bug fix
//include any user-created C headers here
#include "smartnet_types.h"
#include "smartnet_deinterleave.h"
#include "smartnet_crc.h"
#include "smartnet_subchannel_framer.h"
#include "smartnet_wavsink.h"
#include <stdexcept>
%}

GR_SWIG_BLOCK_MAGIC(smartnet,deinterleave);

smartnet_deinterleave_sptr smartnet_make_deinterleave();

class smartnet_deinterleave : public gr_block
{
private:
	smartnet_deinterleave();

public:
};

GR_SWIG_BLOCK_MAGIC(smartnet,crc);

smartnet_crc_sptr smartnet_make_crc(gr_msg_queue_sptr queue);

class smartnet_crc : public gr_sync_block
{
private:
	smartnet_crc(gr_msg_queue_sptr queue);

public:
};

GR_SWIG_BLOCK_MAGIC(smartnet,subchannel_framer);

smartnet_subchannel_framer_sptr smartnet_make_subchannel_framer();

class smartnet_subchannel_framer : public gr_sync_block
{
private:
	smartnet_subchannel_framer();

public:
};

GR_SWIG_BLOCK_MAGIC(smartnet,wavsink);

smartnet_wavsink_sptr
smartnet_make_wavsink (const char *filename,
		      int n_channels,
		      unsigned int sample_rate,
		      int bits_per_sample = 16) throw (std::runtime_error);

class smartnet_wavsink : public gr_sync_block
{
protected:
  smartnet_wavsink(const char *filename,
		  int n_channels,
		  unsigned int sample_rate,
		  int bits_per_sample) throw (std::runtime_error);
  
public:
  ~smartnet_wavsink ();
  bool open(const char* filename);
  void close();
  void set_sample_rate(unsigned int sample_rate);
  void set_bits_per_sample(int bits_per_sample);
	float get_time(void);
};

