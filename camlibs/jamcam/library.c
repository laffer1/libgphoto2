/****************************************************************/
/* library.c  - Gphoto2 library for the KBGear JamCam v2 and v3 */
/*                                                              */
/* Copyright (C) 2001 Chris Pinkham                             */
/*                                                              */
/* Author: Chris Pinkham <cpinkham@infi.net>                    */
/*                                                              */
/* This program is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU General Public   */
/* License as published by the Free Software Foundation; either */
/* version 2 of the License, or (at your option) any later      */
/* version.                                                     */
/*                                                              */
/* This program is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied   */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/* PURPOSE.  See the GNU General Public License for more        */
/* details.                                                     */
/*                                                              */
/* You should have received a copy of the GNU General Public    */
/* License along with this program; if not, write to the Free   */
/* Software Foundation, Inc., 59 Temple Place, Suite 330,       */
/* Boston, MA 02111-1307, USA.                                  */
/****************************************************************/

#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "library.h"

struct jamcam_file jamcam_files[1024];
static int jamcam_count = 0;
static int jamcam_mmc_card_size = 0;

static int jamcam_set_usb_mem_pointer( Camera *camera, int position ) {
	char reply[4];

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_set_usb_mem_pointer");
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "*** position:  %d (0x%x)",
		position, position);

	gp_port_usb_msg_write( camera->port,
		0xa1,
		( position       ) & 0xffff,
		( position >> 16 ) & 0xffff,
		NULL, 0 );

	gp_port_usb_msg_read( camera->port,
		0xa0,
		0,
		0,
		reply, 4 );

	return( GP_OK );
}


/* get the number of images on the mmc card */
static int jamcam_mmc_card_file_count (Camera *camera) {
	char buf[16];
	unsigned char reply[512];
	unsigned int position = 0x40000000;
	int data_incr;
	int width;
	int height;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_mmc_card_file_count");

	memset( buf, 0, sizeof( buf ));

	switch( camera->port->type ) {
		default:
		case GP_PORT_SERIAL:
			strcpy( buf, "KB00" );
			buf[4] = ( position       ) & 0xff;
			buf[5] = ( position >>  8 ) & 0xff;
			buf[6] = ( position >> 16 ) & 0xff;
			buf[7] = ( position >> 24 ) & 0xff;
			jamcam_write_packet( camera, buf, 8 );

			jamcam_read_packet( camera, reply, 16 );

			while( strncmp( reply, "KB", 2 ) == 0 ) {
				width  = (reply[5] * 256) + reply[4];
				height = (reply[7] * 256) + reply[6];

				data_incr = 0;
				data_incr += reply[8];
				data_incr += reply[9] * 256;
				data_incr += reply[10] * 256 * 256;
				data_incr += reply[11] * 256 * 256 * 256;

				jamcam_files[jamcam_count].position = position;
				jamcam_files[jamcam_count].width = width;
				jamcam_files[jamcam_count].height = height;
				jamcam_files[jamcam_count].data_incr = data_incr;

				jamcam_count++;

				position += data_incr;

				buf[4] = ( position       ) & 0xff;
				buf[5] = ( position >>  8 ) & 0xff;
				buf[6] = ( position >> 16 ) & 0xff;
				buf[7] = ( position >> 24 ) & 0xff;
				jamcam_write_packet( camera, buf, 8 );
			
				jamcam_read_packet( camera, reply, 16 );
			}
			break;

		case GP_PORT_USB:
			gp_port_usb_msg_write( camera->port,
				0xa5,
				0x0005,
				0x0000,
				NULL, 0 );

			jamcam_set_usb_mem_pointer( camera, position );

			CHECK( gp_port_read (camera->port, reply, 0x10 ));

			width  = (reply[13] * 256) + reply[12];
			height = (reply[15] * 256) + reply[14];

			jamcam_set_usb_mem_pointer( camera, position + 8 );

			CHECK( gp_port_read (camera->port, reply, 0x200 ));

			gp_port_usb_msg_write( camera->port,
				0xa5,
				0x0006,
				0x0000,
				NULL, 0 );

			while(((unsigned char)reply[0] != 0xff ) &&
			      ((unsigned char)reply[0] != 0xaa )) {
				data_incr = 0;
				data_incr += reply[0];
				data_incr += reply[1] * 256;
				data_incr += reply[2] * 256 * 256;
				data_incr += reply[3] * 256 * 256 * 256;

				jamcam_files[jamcam_count].position = position;
				jamcam_files[jamcam_count].width = width;
				jamcam_files[jamcam_count].height = height;
				jamcam_files[jamcam_count].data_incr = data_incr;
				jamcam_count++;

				position += data_incr;

				gp_port_usb_msg_write( camera->port,
					0xa5,
					0x0005,
					0x0000,
					NULL, 0 );

				jamcam_set_usb_mem_pointer( camera, position );

				CHECK( gp_port_read (camera->port, reply, 0x10 ));

				width  = (reply[13] * 256) + reply[12];
				height = (reply[15] * 256) + reply[14];

				jamcam_set_usb_mem_pointer( camera, position + 8 );

				CHECK( gp_port_read (camera->port, reply, 0x200 ));

				gp_port_usb_msg_write( camera->port,
					0xa5,
					0x0006,
					0x0000,
					NULL, 0 );
			}
			break;
	}

	gp_debug_printf (GP_DEBUG_LOW, "jamcam",
		"*** returning with jamcam_count = %d", jamcam_count);
	return( 0 );
}

int jamcam_file_count (Camera *camera) {
	char buf[16];
	unsigned char reply[16];
	int position = 0;
	int data_incr;
	int width;
	int height;
	int last_offset_size = 0;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_file_count");

	jamcam_count = 0;

	memset( buf, 0, sizeof( buf ));

	switch( camera->port->type ) {
		default:
		case GP_PORT_SERIAL:
			strcpy( buf, "KB00" );
			buf[4] = ( position       ) & 0xff;
			buf[5] = ( position >>  8 ) & 0xff;
			buf[6] = ( position >> 16 ) & 0xff;
			buf[7] = ( position >> 24 ) & 0xff;
			jamcam_write_packet( camera, buf, 8 );

			jamcam_read_packet( camera, reply, 16 );

			while( reply[0] != 0xff ) {
				width  = (reply[5] * 256) + reply[4];
				height = (reply[7] * 256) + reply[6];

				data_incr = 0;
				data_incr += reply[8];
				data_incr += reply[9] * 256;
				data_incr += reply[10] * 256 * 256;
				data_incr += reply[11] * 256 * 256 * 256;

				last_offset_size = data_incr;

				jamcam_files[jamcam_count].position = position;
				jamcam_files[jamcam_count].width = width;
				jamcam_files[jamcam_count].height = height;
				jamcam_files[jamcam_count].data_incr = data_incr;

				jamcam_count++;

				position += data_incr;

				buf[4] = ( position       ) & 0xff;
				buf[5] = ( position >>  8 ) & 0xff;
				buf[6] = ( position >> 16 ) & 0xff;
				buf[7] = ( position >> 24 ) & 0xff;
				jamcam_write_packet( camera, buf, 8 );
			
				jamcam_read_packet( camera, reply, 16 );
			}

			/* the v3 camera uses 0x3fdf0 data increments so check for MMC */
			if ( last_offset_size == 0x03fdf0 ) {
				jamcam_query_mmc_card( camera );
			}
			break;

		case GP_PORT_USB:
			jamcam_set_usb_mem_pointer( camera, position );

			CHECK( gp_port_read (camera->port, reply, 0x10 ));

			width  = (reply[13] * 256) + reply[12];
			height = (reply[15] * 256) + reply[14];

			jamcam_set_usb_mem_pointer( camera, position + 8 );

			CHECK( gp_port_read (camera->port, reply, 0x10 ));

			while((unsigned char)reply[0] != 0xff ) {
				data_incr = 0;
				data_incr += reply[0];
				data_incr += reply[1] * 256;
				data_incr += reply[2] * 256 * 256;
				data_incr += reply[3] * 256 * 256 * 256;

				jamcam_files[jamcam_count].position = position;
				jamcam_files[jamcam_count].width = width;
				jamcam_files[jamcam_count].height = height;
				jamcam_files[jamcam_count].data_incr = data_incr;
				jamcam_count++;

				position += data_incr;

				jamcam_set_usb_mem_pointer( camera, position );

				CHECK( gp_port_read (camera->port, reply, 0x10 ));

				width  = (reply[13] * 256) + reply[12];
				height = (reply[15] * 256) + reply[14];

				jamcam_set_usb_mem_pointer( camera, position + 8 );

				CHECK( gp_port_read (camera->port, reply, 0x10 ));
			}
			break;
	}

	if ( jamcam_mmc_card_size ) {
		jamcam_count += jamcam_mmc_card_file_count( camera );
	}

	gp_debug_printf (GP_DEBUG_LOW, "jamcam",
		"*** returning jamcam_count = %d", jamcam_count);
	return( jamcam_count );
}

int jamcam_fetch_memory( Camera *camera, char *data, int start, int length ) {
	char packet[16];
	int new_start;
	int new_end;
	int bytes_read = 0;
	int bytes_to_read;
	int bytes_left = length;
	float percentage;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_fetch_memory");
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "  * start:  %d (0x%x)",
		start, start);
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "  * length: %d (0x%x)",
		length, length);

	while( bytes_left ) {
		switch( camera->port->type ) {
			default:
			case GP_PORT_SERIAL:
				bytes_to_read =
					bytes_left > SER_PKT_SIZE ? SER_PKT_SIZE : bytes_left;

				memset( packet, 0, sizeof( packet ));
				strcpy( packet, "KB01" );

				new_start = start + bytes_read;
				new_end   = start + bytes_read + bytes_to_read - 1;

				/* start */
				packet[4] = ( new_start      ) & 0xff;
				packet[5] = ( new_start >>  8 ) & 0xff;
				packet[6] = ( new_start >> 16 ) & 0xff;
				packet[7] = ( new_start >> 24 ) & 0xff;

				/* end (inclusive) */
				packet[8]  = ( new_end       ) & 0xff;
				packet[9]  = ( new_end >>  8 ) & 0xff;
				packet[10] = ( new_end >> 16 ) & 0xff;
				packet[11] = ( new_end >> 24 ) & 0xff;

				jamcam_write_packet( camera, packet, 12 );

				CHECK (jamcam_read_packet( camera, data + bytes_read,
					bytes_to_read ));
				break;
			case GP_PORT_USB:
				bytes_to_read = bytes_left > USB_PKT_SIZE ? USB_PKT_SIZE : bytes_left;
				jamcam_set_usb_mem_pointer( camera, start + bytes_read );
				CHECK( gp_port_read (camera->port, data + bytes_read, bytes_to_read ));
				break;
		}
				
		bytes_left -= bytes_to_read;
		bytes_read += bytes_to_read;

		/* hate this hardcoded, but don't want to update here */
		/* when downloading parts of a thumbnail              */
		if ( length > 1000 ) {
			percentage = bytes_read / length;
			gp_camera_progress( camera, percentage );
		}
	}

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_fetch_memory OK");
	return( GP_OK );
}

int jamcam_request_image( Camera *camera, char *buf, int *len, int number ) {
	int position;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_request_image");

	*len = DATA_SIZE;

	position = jamcam_files[number].position + 0x10;
	*len = jamcam_files[number].width * jamcam_files[number].height;

	if ( camera->port->type == GP_PORT_USB ) {
		jamcam_set_usb_mem_pointer( camera, position );
		CHECK( gp_port_read (camera->port, buf, 120 ));

		position += 8;
	}

	return( jamcam_fetch_memory( camera, buf, position, *len ));
}

struct jamcam_file *jamcam_file_info(Camera *camera, int number)
{
	return( &jamcam_files[number] );
}

int jamcam_request_thumbnail( Camera *camera, char *buf, int *len, int number ) {
	char line[600];
	char packet[16];
	int position;
	int x, y;
	char *ptr;
	float percentage;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_request_thumbnail");

	memset( packet, 0, sizeof( packet ));

	position = jamcam_files[number].position + 0x10;

	*len = 4800;

	ptr = buf;

	/* fetch thumbnail lines and build the thumbnail */
	position += 10 * jamcam_files[number].width;
	for( y = 0 ; y < 60 ; y++ ) {
		jamcam_fetch_memory( camera, line, position,
			jamcam_files[number].width );

		percentage = y / 60;
		gp_camera_progress( camera, percentage );

		if ( jamcam_files[number].width == 600 ) {
			for( x = 22; x < 578 ; x += 7 ) {
				*(ptr++) = line[x];
			}
			position += 7 * 600;
		} else {
			for( x = 0; x < 320 ; ) {
				*(ptr++) = line[x];
				x += 3;
				*(ptr++) = line[x];
				x += 5;
			}

			if ( y % 2 ) {
				position += 5 * 320;
			} else {
				position += 3 * 320;
			}
		}
	}

	return( GP_OK );
}

int jamcam_write_packet (Camera *camera, char *packet, int length) {
	int ret, r;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_write_packet");

	for (r = 0; r < RETRIES; r++) {
		ret = gp_port_write (camera->port, packet, length);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;

		return (ret);
	}

	return (GP_ERROR_IO_TIMEOUT);
}


int jamcam_read_packet (Camera *camera, char *packet, int length) {
	int r = 0;
	int bytes_read;

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_read_packet");
	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "*** length: %d (0x%x)",
		length, length);

	for (r = 0; r < RETRIES; r++) {
		bytes_read = gp_port_read (camera->port, packet, length);
		if (bytes_read == GP_ERROR_IO_TIMEOUT)
			continue;
		if (bytes_read < 0)
			return (bytes_read);

		if ( bytes_read == length ) {
			return( GP_OK );
		}
	}

	return (GP_ERROR_IO_TIMEOUT);
}


int jamcam_enq (Camera *camera)
{
	int ret, r = 0;
	unsigned char buf[16];

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_enq");

	memset( buf, 0, 16 );

	switch( camera->port->type ) {
		default:
		case GP_PORT_SERIAL:
			strcpy((char *)buf, "KB99" );

			for (r = 0; r < RETRIES; r++) {

				ret = jamcam_write_packet (camera, (char *)buf, 4);
				if (ret == GP_ERROR_IO_TIMEOUT)
					continue;
				if (ret != GP_OK)
					return (ret);

				ret = jamcam_read_packet (camera, (char *)buf, 4);
				if (ret == GP_ERROR_IO_TIMEOUT)
					continue;
				if (ret != GP_OK)
					return (ret);

				if ( !strncmp( (char *)buf, "KIDB", 4 ))
					/* OK, so query mmc card size, and return result of that */
					/* disabled for now until can autodetect camera version 
					return( jamcam_query_mmc_card( camera ));
					*/
					return (GP_OK);
				else
					return (GP_ERROR_CORRUPTED_DATA);
			
			}
			break;

		case GP_PORT_USB:
			gp_port_usb_msg_write( camera->port,
				0xa5,
				0x0004,
				0x0000,
				NULL, 0 );
			jamcam_set_usb_mem_pointer( camera, 0x0000 );

			CHECK( gp_port_read( camera->port, (char *)buf, 0x0c ));

			if ( !strncmp( (char *)buf, "KB00", 4 )) {
				/* found a JamCam v3 camera */
				/* reply contains 4-bytes showing length of MMC card if any */
				/* set to 0 if none */
				jamcam_mmc_card_size = 0;
				jamcam_mmc_card_size += buf[8];
				jamcam_mmc_card_size += buf[9] * 256;
				jamcam_mmc_card_size += buf[10] * 256 * 256;
				jamcam_mmc_card_size += buf[11] * 256 * 256 * 256;

				if ( jamcam_mmc_card_size ) {
					gp_debug_printf (GP_DEBUG_LOW, "jamcam",
						"* jamcam_enq, MMC card size = %d",
						jamcam_mmc_card_size );
				}

				return (GP_OK);
			} else if ( !strncmp( (char *)buf + 8, "KB00", 4 )) {
				/* found a JamCam v2 camera */
				/* JamCam v2 doesn't support MMC card so no need to check */
				return (GP_OK);
			} else if (( buf[0] == 0xf0 ) &&
					 ( buf[1] == 0xfd ) &&
					 ( buf[2] == 0x03 )) {
				return( GP_OK );
			} else {
				return (GP_ERROR_CORRUPTED_DATA);
			}

			break;
	}

	return (GP_ERROR_IO_TIMEOUT);
}

int jamcam_query_mmc_card (Camera *camera)
{
	int ret, r = 0;
	char buf[16];

	/* FIXME! JamCam v2 doesn't support MMC card so no need to check */

	gp_debug_printf (GP_DEBUG_LOW, "jamcam", "* jamcam_query_mmc_card");

	/* usb port doesn't need this packet, this info found in enquiry reply */
	if ( camera->port->type == GP_PORT_USB ) {
		return( GP_OK );
	}

	strcpy( buf, "KB04" );

	for (r = 0; r < RETRIES; r++) {

		ret = jamcam_write_packet (camera, buf, 4);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;
		if (ret != GP_OK)
			return (ret);

		ret = jamcam_read_packet (camera, buf, 4);
		if (ret == GP_ERROR_IO_TIMEOUT)
			continue;
		if (ret != GP_OK)
			return (ret);

		/* reply is 4-byte int showing length of MMC card if any, 0 if none */
		jamcam_mmc_card_size = 0;
		jamcam_mmc_card_size += buf[0];
		jamcam_mmc_card_size += buf[1] * 256;
		jamcam_mmc_card_size += buf[2] * 256 * 256;
		jamcam_mmc_card_size += buf[3] * 256 * 256 * 256;

		if ( jamcam_mmc_card_size ) {
			gp_debug_printf (GP_DEBUG_LOW, "jamcam",
				"* jamcam_query_mmc_card, MMC card size = %d",
				jamcam_mmc_card_size );
		}

		return (GP_OK);
	}
	return (GP_ERROR_IO_TIMEOUT);
}

