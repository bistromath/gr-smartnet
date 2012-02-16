//smartnet_crc.cc
/* -*- c++ -*- */
/*
 * Copyright 2012 Nick Foster
 * 
 * This file is part of gr_smartnet
 * 
 * gr_smartnet is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * gr_smartnet is distributed in the hope that it will be useful,
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

#include "smartnet_crc.h"
#include <gr_io_signature.h>
#include <stdio.h>
#include <gr_tags.h>

#define VERBOSE 1

/*
 * Create a new instance of smartnet_crc and return
 * a boost shared_ptr.  This is effectively the public constructor.
 */
smartnet_crc_sptr smartnet_make_crc()
{
  return smartnet_crc_sptr (new smartnet_crc ());
}

/*
 * Specify constraints on number of input and output streams.
 * This info is used to construct the input and output signatures
 * (2nd & 3rd args to gr_block's constructor).  The input and
 * output signatures are used by the runtime system to
 * check that a valid number and type of inputs and outputs
 * are connected to this block.  In this case, we accept
 * only 1 input and 1 output.
 */
static const int MIN_IN = 1;    // mininum number of input streams
static const int MAX_IN = 1;    // maximum number of input streams
static const int MIN_OUT = 1;   // minimum number of output streams
static const int MAX_OUT = 1;   // maximum number of output streams

/*
 * The private constructor
 */
smartnet_crc::smartnet_crc ()
  : gr_sync_block ("crc",
                   gr_make_io_signature (MIN_IN, MAX_IN, sizeof (char)),
                   gr_make_io_signature (MIN_OUT, MAX_OUT, sizeof (char)))
{
    //nothing else required in this example
    set_output_multiple(38);
}

/*
 * Our virtual destructor.
 */
smartnet_crc::~smartnet_crc ()
{
    //nothing else required in this example
}

int 
smartnet_crc::work (int noutput_items,
		    gr_vector_const_void_star &input_items,
		    gr_vector_void_star &output_items)
{
    const char *in = (const char *) input_items[0];
    char *out = (char *) output_items[0];

    int size = noutput_items - 38;
    if(size <= 0) {
	return 0; //better luck next time
    }

    uint64_t abs_sample_cnt = nitems_read(0);
    std::vector<gr_tag_t> frame_tags;

    get_tags_in_range(frame_tags, 0, abs_sample_cnt, abs_sample_cnt + size, pmt::pmt_string_to_symbol("smartnet_frame"));
    if(frame_tags.size() == 0) {
	return 0; //sad trombone
    }

    std::vector<gr_tag_t>::iterator tag_iter;
    for(tag_iter = frame_tags.begin(); tag_iter != frame_tags.end(); tag_iter++) {
	uint64_t mark = tag_iter->offset - abs_sample_cnt;
	if(VERBOSE) std::cout << "found a frame at " << mark << std::endl;


	unsigned int crcaccum = 0x0393;
	unsigned int crcop = 0x036E;
	unsigned int crcgiven;

	//calc expected crc
	for(int j=0; j<27; j++) {
	    if(crcop & 0x01) crcop = (crcop >> 1)^0x0225;
	    else crcop >>= 1;
	}

	//load given crc
	crcgiven = 0x0000;
	for(int j=0; j<10; j++) {
	    crcgiven <<= 1;
	    crcgiven += !bool(in[mark+j+27] & 0x01);
	}

	if(VERBOSE) if(crcgiven != crcaccum) std::cout << "Failed CRC!" << std::endl;

	if(crcgiven == crcaccum) {
	    //send a tag downstream or push a message or something
	}
    }

    for(int j=0; j<size; j++) out[j] = in[j];
    
    return size;
}
    

  while(i < noutput_items) {
	while(!(in[i] & 0x02)) i++; //skip until the start bit is found
	if((noutput_items-i) < 38) return i;

	crcaccum = 0x0393;
	crcop = 0x036E;

	//now we're at the start of a 38-bit frame, so calculate the expected crc
	for(int j = 0; j < 27; j++) {
		if (crcop & 0x01) crcop = (crcop >> 1) ^ 0x0225; else crcop = crcop >> 1;
		if (in[i+j] & 0x01) crcaccum = crcaccum ^ crcop;
	}

	//now we load up the GIVEN crc value
	crcgiven = 0x0000;
	for(int j = 0; j < 10; j++) {
		crcgiven <<= 1;
		crcgiven += !bool(in[i+j+27] & 0x01);
	}

	if(crcgiven != crcaccum) printf("Failed CRC!\n");	

	for(int j = 0; j < 38; j++) {
		if(crcgiven == crcaccum) out[j+i] = in[j+i];
		else out[j+i] = in[j+i] & 0x01;
	}

	i += 38;
  }

  // Tell runtime system how many output items we produced.
  return noutput_items;
}

