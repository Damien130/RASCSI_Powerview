//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//  Copyright (C) 2020 akuker
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//
//  Licensed under the BSD 3-Clause License. 
//  See LICENSE file in the project root folder.
//
//  [ Emulation of the DaynaPort SCSI Link Ethernet interface ]
//
//  This design is derived from the SLINKCMD.TXT file, as well as David Kuder's 
//  Tiny SCSI Emulator
//    - SLINKCMD: http://www.bitsavers.org/pdf/apple/scsi/dayna/daynaPORT/SLINKCMD.TXT
//    - Tiny SCSI : https://hackaday.io/project/18974-tiny-scsi-emulator
//
//  Additional documentation and clarification is available at the 
//  following link:
//    - https://github.com/akuker/RASCSI/wiki/Dayna-Port-SCSI-Link
//
//  This does NOT include the file system functionality that is present
//  in the Sharp X68000 host bridge.
//
//  Note: This requires a DaynaPort SCSI Link driver.
//---------------------------------------------------------------------------

#include "scsi_powerview.h"
#include <sstream>


#include <err.h>
#include <fcntl.h>
#include <linux/fb.h>
#include "os.h"
#include "disk.h"
#include <sys/mman.h>
#include "log.h"

static unsigned char reverse_table[256];

// const BYTE SCSIPowerView::m_bcast_addr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
// const BYTE SCSIPowerView::m_apple_talk_addr[6] = { 0x09, 0x00, 0x07, 0xff, 0xff, 0xff };

const BYTE SCSIPowerView::m_inquiry_response[] = {
0x03, 0x00, 0x01, 0x01, 0x46, 0x00, 0x00, 0x00, 0x52, 0x41, 0x44, 0x49, 0x55, 0x53, 0x20, 0x20,
0x50, 0x6F, 0x77, 0x65, 0x72, 0x56, 0x69, 0x65, 0x77, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
0x56, 0x31, 0x2E, 0x30, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x20, 0x00, 0x01, 0x00, 0x00, 0x00,
0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
0x05, 0x00, 0x00, 0x00, 0x00, 0x06, 0x43, 0xF9, 0x00, 0x00, 0xFF,
};

SCSIPowerView::SCSIPowerView() : Disk("SCPV")
{
	AddCommand(SCSIDEV::eCmdUnknownPowerViewC8, "Unknown PowerViewC8", &SCSIPowerView::UnknownCommandC8);
	AddCommand(SCSIDEV::eCmdUnknownPowerViewC9, "Unknown PowerViewC9", &SCSIPowerView::UnknownCommandC9);
	AddCommand(SCSIDEV::eCmdUnknownPowerViewCA, "Unknown PowerViewCA", &SCSIPowerView::UnknownCommandCA);
	AddCommand(SCSIDEV::eCmdUnknownPowerViewCB, "Unknown PowerViewCB", &SCSIPowerView::UnknownCommandCB);
	AddCommand(SCSIDEV::eCmdUnknownPowerViewCC, "Unknown PowerViewCC", &SCSIPowerView::UnknownCommandCC);

	struct fb_var_screeninfo fbinfo;
	struct fb_fix_screeninfo fbfixinfo;

	// create lookup table
	for (int i = 0; i < 256; i++) {
		unsigned char b = i;
		b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
		b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
		b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
		reverse_table[i] = b;
	}

	// TODO: receive these through a SCSI message sent by the remote
	this->screen_width = 512;
	this->screen_height = 342;

	this->fbfd = open("/dev/fb0", O_RDWR);
	if (this->fbfd == -1)
		err(1, "open /dev/fb0");

	if (ioctl(this->fbfd, FBIOGET_VSCREENINFO, &fbinfo))
		err(1, "ioctl FBIOGET_VSCREENINFO");

	if (ioctl(this->fbfd, FBIOGET_FSCREENINFO, &fbfixinfo))
		err(1, "ioctl FBIOGET_FSCREENINFO");

	// if (fbinfo.bits_per_pixel != 32)
	// 	errx(1, "TODO: support %d bpp", fbinfo.bits_per_pixel);

	this->fbwidth = fbinfo.xres;
	this->fbheight = fbinfo.yres;
	this->fbbpp = fbinfo.bits_per_pixel;
	this->fblinelen = fbfixinfo.line_length;
	this->fbsize = fbfixinfo.smem_len;

	printf("SCSIVideo drawing on %dx%d %d bpp framebuffer\n",
	    this->fbwidth, this->fbheight, this->fbbpp);

	this->fb = (char *)mmap(0, this->fbsize, PROT_READ | PROT_WRITE, MAP_SHARED,
		this->fbfd, 0);
	if ((int)this->fb == -1)
		err(1, "mmap");

	memset(this->fb, 0, this->fbsize);

}

SCSIPowerView::~SCSIPowerView()
{
	// // TAP driver release
	// if (m_tap) {
	// 	m_tap->Cleanup();
	// 	delete m_tap;
	// }

    	munmap(this->fb, this->fbsize);
	close(this->fbfd);

	for (auto const& command : commands) {
		delete command.second;
	}
}

void SCSIPowerView::AddCommand(SCSIDEV::scsi_command opcode, const char* name, void (SCSIPowerView::*execute)(SASIDEV *))
{
	commands[opcode] = new command_t(name, execute);
}


void SCSIPowerView::dump_command(SASIDEV *controller){

            LOGWARN("                %02X %02X %02X %02X %02X %02X %02X %02X [%02X] \n",
				ctrl->cmd[0],
				ctrl->cmd[1],
				ctrl->cmd[2],
				ctrl->cmd[3],
				ctrl->cmd[4],
				ctrl->cmd[5],
				ctrl->cmd[6],
				ctrl->cmd[7],
				ctrl->cmd[8]);

	// for(int i=0; i<8; i++){
    //         LOGWARN("   [%d]: %08X\n",i, ctrl->cmd[i]);	
	// }

}

//---------------------------------------------------------------------------
//
//	Unknown Command C8
//
//---------------------------------------------------------------------------
void SCSIPowerView::UnknownCommandC8(SASIDEV *controller)
{

	// Set transfer amount
	ctrl->length = ctrl->cmd[6];
	LOGWARN("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);
	dump_command(controller);

	if (ctrl->length <= 0) {
		// Failure (Error)
		controller->Error();
		return;
	}

	ctrl->buffer[0] = 0x01;
	ctrl->buffer[1] = 0x09;
	ctrl->buffer[2] = 0x08;

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	controller->DataIn();
}


//---------------------------------------------------------------------------
//
//	Unknown Command C9
//
//---------------------------------------------------------------------------
void SCSIPowerView::UnknownCommandC9(SASIDEV *controller)
{

	// Set transfer amount
	ctrl->length = ctrl->cmd[6];
	LOGWARN("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);
	dump_command(controller);
	// LOGWARN("Controller: %08X ctrl: %08X", (DWORD)controller->GetCtrl(), (DWORD)ctrl);
	// if (ctrl->length <= 0) {
	// 	// Failure (Error)
	// 	controller->Error();
	// 	return;
	// }

	if (ctrl->length == 0){
		controller->Status();
	}
	else
	{
		// Set next block
		ctrl->blocks = 1;
		ctrl->next = 1;

		controller->DataOut();
	}
}



//---------------------------------------------------------------------------
//
//	Unknown Command CA
//
//---------------------------------------------------------------------------
void SCSIPowerView::UnknownCommandCA(SASIDEV *controller)
{

	// Set transfer amount
	// ctrl->length = ctrl->cmd[6];
	uint16_t width_x = ctrl->cmd[5] + (ctrl->cmd[4] << 8);
	uint16_t height_y = ctrl->cmd[7] + (ctrl->cmd[6] << 8);

	ctrl->length = width_x * height_y;
	// if(ctrl->cmd[9] == 0){
	// 	ctrl->length = 0x9600;
	// }
	// else {
	// 	ctrl->length = ctrl->cmd[7] * 2;
	// }
	LOGWARN("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);
	LOGWARN("                %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X [%02X %02X]\n",
				ctrl->cmd[0],
				ctrl->cmd[1],
				ctrl->cmd[2],
				ctrl->cmd[3],
				ctrl->cmd[4],
				ctrl->cmd[5],
				ctrl->cmd[6],
				ctrl->cmd[7],
				ctrl->cmd[8],
				ctrl->cmd[9],
				ctrl->cmd[10],
				ctrl->cmd[11],
				ctrl->cmd[12]);


	if (ctrl->length <= 0) {
		// Failure (Error)
		controller->Error();
		return;
	}

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	controller->DataOut();
}


//---------------------------------------------------------------------------
//
//	Unknown Command CB
//
//---------------------------------------------------------------------------
void SCSIPowerView::UnknownCommandCB(SASIDEV *controller)
{

	// Set transfer amount
	ctrl->length = ctrl->cmd[6];
	ctrl->length = ctrl->length * 4;
	LOGWARN("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);
	dump_command(controller);
	if (ctrl->length <= 0) {
		// Failure (Error)
		controller->Error();
		return;
	}

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	controller->DataOut();
}


//---------------------------------------------------------------------------
//
//	Unknown Command CC
//
//---------------------------------------------------------------------------
void SCSIPowerView::UnknownCommandCC(SASIDEV *controller)
{

	// Set transfer amount
	// ctrl->length = ctrl->cmd[6];
	ctrl->length = 0x8bb;
	LOGWARN("%s Message Length %d", __PRETTY_FUNCTION__, (int)ctrl->length);
	dump_command(controller);
	if (ctrl->length <= 0) {
		// Failure (Error)
		controller->Error();
		return;
	}

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	controller->DataOut();
}

bool SCSIPowerView::Dispatch(SCSIDEV *controller)
{
	ctrl = controller->GetCtrl();

	if (commands.count(static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0]))) {
		command_t *command = commands[static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0])];

		LOGDEBUG("%s Executing %s ($%02X)", __PRETTY_FUNCTION__, command->name, (unsigned int)ctrl->cmd[0]);

		(this->*command->execute)(controller);

		return true;
	}

	LOGTRACE("%s Calling base class for dispatching $%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl->cmd[0]);

	// The base class handles the less specific commands
	return Disk::Dispatch(controller);
}

bool SCSIPowerView::Init(const map<string, string>& params)
{
	SetParams(params.empty() ? GetDefaultParams() : params);

// #ifdef __linux__
// 	// TAP Driver Generation
// 	m_tap = new CTapDriver(GetParam("interfaces"));
// 	m_bTapEnable = m_tap->Init();
// 	if(!m_bTapEnable){
// 		LOGERROR("Unable to open the TAP interface");

// // Not terminating on regular Linux PCs is helpful for testing
// #if !defined(__x86_64__) && !defined(__X86__)
// 		return false;
// #endif
// 	} else {
// 		LOGDEBUG("Tap interface created");
// 	}

// 	this->Reset();
// 	SetReady(true);
// 	SetReset(false);

// 	// Generate MAC Address
// 	memset(m_mac_addr, 0x00, 6);

// 	// if (m_bTapEnable) {
// 	// 	tap->GetMacAddr(m_mac_addr);
// 	// 	m_mac_addr[5]++;
// 	// }
// 	// !!!!!!!!!!!!!!!!! For now, hard code the MAC address. Its annoying when it keeps changing during development!
// 	// TODO: Remove this hard-coded address
// 	m_mac_addr[0]=0x00;
// 	m_mac_addr[1]=0x80;
// 	m_mac_addr[2]=0x19;
// 	m_mac_addr[3]=0x10;
// 	m_mac_addr[4]=0x98;
// 	m_mac_addr[5]=0xE3;
// #endif	// linux

	return true;
}

void SCSIPowerView::Open(const Filepath& path)
{
	// m_tap->OpenDump(path);
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int SCSIPowerView::Inquiry(const DWORD *cdb, BYTE *buf)
{
	int allocation_length = cdb[4] + (((DWORD)cdb[3]) << 8);
	if (allocation_length > 0) {
		if ((unsigned int)allocation_length > sizeof(m_inquiry_response)) {
			allocation_length = (int)sizeof(m_inquiry_response);
		}

		memcpy(buf, m_inquiry_response, allocation_length);

		// // Basic data
		// // buf[0] ... Processor Device
		// // buf[1] ... Not removable
		// // buf[2] ... SCSI-2 compliant command system
		// // buf[3] ... SCSI-2 compliant Inquiry response
		// // buf[4] ... Inquiry additional data
		// // http://www.bitsavers.org/pdf/apple/scsi/dayna/daynaPORT/pocket_scsiLINK/pocketscsilink_inq.png
		// memset(buf, 0, allocation_length);
		// buf[0] = 0x03;
		// buf[2] = 0x01;
		// buf[4] = 0x1F;

		// // Padded vendor, product, revision
		// memcpy(&buf[8], GetPaddedName().c_str(), 28);
	}

	LOGTRACE("response size is %d", allocation_length);

	return allocation_length;
}



// //---------------------------------------------------------------------------
// //
// //	INQUIRY
// //
// //---------------------------------------------------------------------------
// int SCSIHD::Inquiry(const DWORD *cdb, BYTE *buf)
// {
// 	ASSERT(cdb);
// 	ASSERT(buf);

// 	// EVPD check
// 	if (cdb[1] & 0x01) {
// 		SetStatusCode(STATUS_INVALIDCDB);
// 		return 0;
// 	}

// 	// Ready check (Error if no image file)
// 	if (!IsReady()) {
// 		SetStatusCode(STATUS_NOTREADY);
// 		return 0;
// 	}

// 	// Basic data
// 	// buf[0] ... Direct Access Device
// 	// buf[1] ... Bit 7 set means removable
// 	// buf[2] ... SCSI-2 compliant command system
// 	// buf[3] ... SCSI-2 compliant Inquiry response
// 	// buf[4] ... Inquiry additional data
// 	memset(buf, 0, 8);
// 	buf[1] = IsRemovable() ? 0x80 : 0x00;
// 	buf[2] = 0x02;
// 	buf[3] = 0x02;
// 	buf[4] = 28 + 3;	// Value close to real HDD

// 	// Padded vendor, product, revision
// 	memcpy(&buf[8], GetPaddedName().c_str(), 28);

// 	// Size of data that can be returned
// 	int size = (buf[4] + 5);

// 	// Limit if the other buffer is small
// 	if (size > (int)cdb[4]) {
// 		size = (int)cdb[4];
// 	}

// 	return size;
// }




//---------------------------------------------------------------------------
//
//	READ
//
//   Command:  08 00 00 LL LL XX (LLLL is data length, XX = c0 or 80)
//   Function: Read a packet at a time from the device (standard SCSI Read)
//   Type:     Input; the following data is returned:
//             LL LL NN NN NN NN XX XX XX ... CC CC CC CC
//   where:
//             LLLL      is normally the length of the packet (a 2-byte
//                       big-endian hex value), including 4 trailing bytes
//                       of CRC, but excluding itself and the flag field.
//                       See below for special values
//             NNNNNNNN  is a 4-byte flag field with the following meanings:
//                       FFFFFFFF  a packet has been dropped (?); in this case
//                                 the length field appears to be always 4000
//                       00000010  there are more packets currently available
//                                 in SCSI/Link memory
//                       00000000  this is the last packet
//             XX XX ... is the actual packet
//             CCCCCCCC  is the CRC
//
//   Notes:
//    - When all packets have been retrieved successfully, a length field
//      of 0000 is returned; however, if a packet has been dropped, the
//      SCSI/Link will instead return a non-zero length field with a flag
//      of FFFFFFFF when there are no more packets available.  This behaviour
//      seems to continue until a disable/enable sequence has been issued.
//    - The SCSI/Link apparently has about 6KB buffer space for packets.
//
//---------------------------------------------------------------------------
int SCSIPowerView::Read(const DWORD *cdb, BYTE *buf, uint64_t block)
{
	int rx_packet_size = 0;
    return rx_packet_size;
	// scsi_resp_read_t *response = (scsi_resp_read_t*)buf;

	// ostringstream s;
	// s << __PRETTY_FUNCTION__ << " reading DaynaPort block " << block;
	// LOGTRACE("%s", s.str().c_str());

	// int requested_length = cdb[4];
	// LOGTRACE("%s Read maximum length %d, (%04X)", __PRETTY_FUNCTION__, requested_length, requested_length);


	// // At host startup, it will send a READ(6) command with a length of 1. We should 
	// // respond by going into the status mode with a code of 0x02
	// if (requested_length == 1) {
	// 	return 0;
	// }

	// // Some of the packets we receive will not be for us. So, we'll keep pulling messages
	// // until the buffer is empty, or we've read X times. (X is just a made up number)
	// bool send_message_to_host;
	// int read_count = 0;
	// while (read_count < MAX_READ_RETRIES) {
	// 	read_count++;

	// 	// The first 2 bytes are reserved for the length of the packet
	// 	// The next 4 bytes are reserved for a flag field
	// 	//rx_packet_size = m_tap->Rx(response->data);
	// 	rx_packet_size = m_tap->Rx(&buf[DAYNAPORT_READ_HEADER_SZ]);

	// 	// If we didn't receive anything, return size of 0
	// 	if (rx_packet_size <= 0) {
	// 		LOGTRACE("%s No packet received", __PRETTY_FUNCTION__);
	// 		response->length = 0;
	// 		response->flags = e_no_more_data;
	// 		return DAYNAPORT_READ_HEADER_SZ;
	// 	}

	// 	LOGTRACE("%s Packet Sz %d (%08X) read: %d", __PRETTY_FUNCTION__, (unsigned int) rx_packet_size, (unsigned int) rx_packet_size, read_count);

	// 	// This is a very basic filter to prevent unnecessary packets from
	// 	// being sent to the SCSI initiator. 
	// 	send_message_to_host = false;

	// // The following doesn't seem to work with unicast messages. Temporarily removing the filtering
	// // functionality.
	// ///////	// Check if received packet destination MAC address matches the
	// ///////	// DaynaPort MAC. For IP packets, the mac_address will be the first 6 bytes
	// ///////	// of the data.
	// ///////	if (memcmp(response->data, m_mac_addr, 6) == 0) {
	// ///////		send_message_to_host = true;
	// ///////	}

	// ///////	// Check to see if this is a broadcast message
	// ///////	if (memcmp(response->data, m_bcast_addr, 6) == 0) {
	// ///////		send_message_to_host = true;
	// ///////	}

	// ///////	// Check to see if this is an AppleTalk Message
	// ///////	if (memcmp(response->data, m_apple_talk_addr, 6) == 0) {
	// ///////		send_message_to_host = true;
	// ///////	}
	// 	send_message_to_host = true;

	// 	// TODO: We should check to see if this message is in the multicast 
	// 	// configuration from SCSI command 0x0D

	// 	if (!send_message_to_host) {
	// 		LOGDEBUG("%s Received a packet that's not for me: %02X %02X %02X %02X %02X %02X", 
	// 			__PRETTY_FUNCTION__,
	// 			(int)response->data[0],
	// 			(int)response->data[1],
	// 			(int)response->data[2],
	// 			(int)response->data[3],
	// 			(int)response->data[4],
	// 			(int)response->data[5]);

	// 		// If there are pending packets to be processed, we'll tell the host that the read
	// 		// length was 0.
	// 		if (!m_tap->PendingPackets())
	// 		{
	// 			response->length = 0;
	// 			response->flags = e_no_more_data;
	// 			return DAYNAPORT_READ_HEADER_SZ;
	// 		}
	// 	}
	// 	else {
	// 		// TODO: Need to do some sort of size checking. The buffer can easily overflow, probably.

	// 		// response->length = rx_packet_size;
	// 		// if(m_tap->PendingPackets()){
	// 		// 	response->flags = e_more_data_available;
	// 		// } else {
	// 		// 	response->flags = e_no_more_data;
	// 		// }
	// 		buf[0] = (BYTE)((rx_packet_size >> 8) & 0xFF);
	// 		buf[1] = (BYTE)(rx_packet_size & 0xFF);

	// 		buf[2] = 0;
	// 		buf[3] = 0;
	// 		buf[4] = 0;
	// 		if(m_tap->PendingPackets()){
	// 			buf[5] = 0x10;
	// 		} else {
	// 			buf[5] = 0;
	// 		}

	// 		// Return the packet size + 2 for the length + 4 for the flag field
	// 		// The CRC was already appended by the ctapdriver
	// 		return rx_packet_size + DAYNAPORT_READ_HEADER_SZ;
	// 	}
	// 	// If we got to this point, there are still messages in the queue, so 
	// 	// we should loop back and get the next one.
	// } // end while

	// response->length = 0;
	// response->flags = e_no_more_data;
	// return DAYNAPORT_READ_HEADER_SZ;
}

// //---------------------------------------------------------------------------
// //
// // WRITE check
// //
// //---------------------------------------------------------------------------
// int SCSIPowerView::WriteCheck(DWORD block)
// {
// 	// // Status check
// 	// if (!CheckReady()) {
// 	// 	return 0;
// 	// }

// 	// if (!m_bTapEnable){
// 	// 	SetStatusCode(STATUS_NOTREADY);
// 	// 	return 0;
// 	// }

// 	// Success
// 	return 1;
// }
	
//---------------------------------------------------------------------------
//
//  Write
//
//   Command:  0a 00 00 LL LL XX (LLLL is data length, XX = 80 or 00)
//   Function: Write a packet at a time to the device (standard SCSI Write)
//   Type:     Output; the format of the data to be sent depends on the value
//             of XX, as follows:
//              - if XX = 00, LLLL is the packet length, and the data to be sent
//                must be an image of the data packet
//              - if XX = 80, LLLL is the packet length + 8, and the data to be
//                sent is:
//                  PP PP 00 00 XX XX XX ... 00 00 00 00
//                where:
//                  PPPP      is the actual (2-byte big-endian) packet length
//               XX XX ... is the actual packet
//
//---------------------------------------------------------------------------
bool SCSIPowerView::Write(const DWORD *cdb, const BYTE *buf, DWORD block)
{
	BYTE data_format = cdb[5];
	// WORD data_length = (WORD)cdb[4] + ((WORD)cdb[3] << 8);

	if (data_format == 0x00){
		// m_tap->Tx(buf, data_length);
		// LOGTRACE("%s Transmitted %u bytes (00 format)", __PRETTY_FUNCTION__, data_length);
		return true;
	}
	else if (data_format == 0x80){
		// // The data length is specified in the first 2 bytes of the payload
		// data_length=(WORD)buf[1] + ((WORD)buf[0] << 8);
		// m_tap->Tx(&buf[4], data_length);
		// LOGTRACE("%s Transmitted %u bytes (80 format)", __PRETTY_FUNCTION__, data_length);
		return true;
	}
	else
	{
		// LOGWARN("%s Unknown data format %02X", __PRETTY_FUNCTION__, (unsigned int)command->format);
		LOGWARN("%s Unknown data format %02X", __PRETTY_FUNCTION__, (unsigned int)data_format);
		return true;
	}
}
	
// //---------------------------------------------------------------------------
// //
// //	RetrieveStats
// //
// //   Command:  09 00 00 00 12 00
// //   Function: Retrieve MAC address and device statistics
// //   Type:     Input; returns 18 (decimal) bytes of data as follows:
// //              - bytes 0-5:  the current hardware ethernet (MAC) address
// //              - bytes 6-17: three long word (4-byte) counters (little-endian).
// //   Notes:    The contents of the three longs are typically zero, and their
// //             usage is unclear; they are suspected to be:
// //              - long #1: frame alignment errors
// //              - long #2: CRC errors
// //              - long #3: frames lost
// //
// //---------------------------------------------------------------------------
// int SCSIPowerView::RetrieveStats(const DWORD *cdb, BYTE *buffer)
// {
// 	int allocation_length = cdb[4] + (((DWORD)cdb[3]) << 8);

// 	// memset(buffer,0,18);
// 	// memcpy(&buffer[0],m_mac_addr,sizeof(m_mac_addr));
// 	// uint32_t frame_alignment_errors;
// 	// uint32_t crc_errors;
// 	// uint32_t frames_lost;
// 	// // frame alignment errors
// 	// frame_alignment_errors = htonl(0);
// 	// memcpy(&(buffer[6]),&frame_alignment_errors,sizeof(frame_alignment_errors));
// 	// // CRC errors
// 	// crc_errors = htonl(0);
// 	// memcpy(&(buffer[10]),&crc_errors,sizeof(crc_errors));
// 	// // frames lost
// 	// frames_lost = htonl(0);
// 	// memcpy(&(buffer[14]),&frames_lost,sizeof(frames_lost));

// 	int response_size = sizeof(m_scsi_link_stats);
// 	memcpy(buffer, &m_scsi_link_stats, sizeof(m_scsi_link_stats));

// 	if (response_size > allocation_length) {
// 		response_size = allocation_length;
// 	}

// 	// Success
// 	return response_size;
// }

// //---------------------------------------------------------------------------
// //
// //	Enable or Disable the interface
// //
// //  Command:  0e 00 00 00 00 XX (XX = 80 or 00)
// //  Function: Enable (80) / disable (00) Ethernet interface
// //  Type:     No data transferred
// //  Notes:    After issuing an Enable, the initiator should avoid sending
// //            any subsequent commands to the device for approximately 0.5
// //            seconds
// //
// //---------------------------------------------------------------------------
// bool SCSIPowerView::EnableInterface(const DWORD *cdb)
// {
// 	bool result;
// 	if (cdb[5] & 0x80) {
// 		result = m_tap->Enable();
// 		if (result) {
// 			LOGINFO("The DaynaPort interface has been ENABLED.");
// 		}
// 		else{
// 			LOGWARN("Unable to enable the DaynaPort Interface");
// 		}
// 		m_tap->Flush();
// 	}
// 	else {
// 		result = m_tap->Disable();
// 		if (result) {
// 			LOGINFO("The DaynaPort interface has been DISABLED.");
// 		}
// 		else{
// 			LOGWARN("Unable to disable the DaynaPort Interface");
// 		}
// 	}

// 	return result;
// }

// void SCSIPowerView::TestUnitReady(SASIDEV *controller)
// {
// 	// TEST UNIT READY Success

// 	controller->Status();
// }

// void SCSIPowerView::Read6(SASIDEV *controller)
// {
// 	// Get record number and block number
// 	uint32_t record = ctrl->cmd[1] & 0x1f;
// 	record <<= 8;
// 	record |= ctrl->cmd[2];
// 	record <<= 8;
// 	record |= ctrl->cmd[3];
// 	ctrl->blocks=1;

// 	// If any commands have a bogus control value, they were probably not
// 	// generated by the DaynaPort driver so ignore them
// 	if (ctrl->cmd[5] != 0xc0 && ctrl->cmd[5] != 0x80) {
// 		LOGTRACE("%s Control value %d, (%04X), returning invalid CDB", __PRETTY_FUNCTION__, ctrl->cmd[5], ctrl->cmd[5]);
// 		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
// 		return;
// 	}

// 	LOGTRACE("%s READ(6) command record=%d blocks=%d", __PRETTY_FUNCTION__, (unsigned int)record, (int)ctrl->blocks);

// 	ctrl->length = Read(ctrl->cmd, ctrl->buffer, record);
// 	LOGTRACE("%s ctrl.length is %d", __PRETTY_FUNCTION__, (int)ctrl->length);

// 	// Set next block
// 	ctrl->next = record + 1;

// 	controller->DataIn();
// }

// void SCSIPowerView::Write6(SASIDEV *controller)
// {
// 	// Reallocate buffer (because it is not transfer for each block)
// 	if (ctrl->bufsize < DAYNAPORT_BUFFER_SIZE) {
// 		free(ctrl->buffer);
// 		ctrl->bufsize = DAYNAPORT_BUFFER_SIZE;
// 		ctrl->buffer = (BYTE *)malloc(ctrl->bufsize);
// 	}

// 	DWORD data_format = ctrl->cmd[5];

// 	if(data_format == 0x00) {
// 		ctrl->length = (WORD)ctrl->cmd[4] + ((WORD)ctrl->cmd[3] << 8);
// 	}
// 	else if (data_format == 0x80) {
// 		ctrl->length = (WORD)ctrl->cmd[4] + ((WORD)ctrl->cmd[3] << 8) + 8;
// 	}
// 	else {
// 		LOGWARN("%s Unknown data format %02X", __PRETTY_FUNCTION__, (unsigned int)data_format);
// 	}
// 	LOGTRACE("%s length: %04X (%d) format: %02X", __PRETTY_FUNCTION__, (unsigned int)ctrl->length, (int)ctrl->length, (unsigned int)data_format);

// 	if (ctrl->length <= 0) {
// 		// Failure (Error)
// 		controller->Error();
// 		return;
// 	}

// 	// Set next block
// 	ctrl->blocks = 1;
// 	ctrl->next = 1;

// 	controller->DataOut();
// }

// void SCSIPowerView::RetrieveStatistics(SASIDEV *controller)
// {
// 	ctrl->length = RetrieveStats(ctrl->cmd, ctrl->buffer);
// 	if (ctrl->length <= 0) {
// 		// Failure (Error)
// 		controller->Error();
// 		return;
// 	}

// 	// Set next block
// 	ctrl->blocks = 1;
// 	ctrl->next = 1;

// 	controller->DataIn();
// }

// //---------------------------------------------------------------------------
// //
// //	Set interface mode/Set MAC address
// //
// //   Set Interface Mode (0c)
// //   -----------------------
// //   Command:  0c 00 00 00 FF 80 (FF = 08 or 04)
// //   Function: Allow interface to receive broadcast messages (FF = 04); the
// //             function of (FF = 08) is currently unknown.
// //   Type:     No data transferred
// //   Notes:    This command is accepted by firmware 1.4a & 2.0f, but has no
// //             effect on 2.0f, which is always capable of receiving broadcast
// //             messages.  In 1.4a, once broadcast mode is set, it remains set
// //             until the interface is disabled.
// //
// //   Set MAC Address (0c)
// //   --------------------
// //   Command:  0c 00 00 00 FF 40 (FF = 08 or 04)
// //   Function: Set MAC address
// //   Type:     Output; overrides built-in MAC address with user-specified
// //             6-byte value
// //   Notes:    This command is intended primarily for debugging/test purposes.
// //             Disabling the interface resets the MAC address to the built-in
// //             value.
// //
// //---------------------------------------------------------------------------
// void SCSIPowerView::SetInterfaceMode(SASIDEV *controller)
// {
// 	// Check whether this command is telling us to "Set Interface Mode" or "Set MAC Address"

// 	ctrl->length = RetrieveStats(ctrl->cmd, ctrl->buffer);
// 	switch(ctrl->cmd[5]){
// 		case SCSIPowerView::CMD_SCSILINK_SETMODE:
// 			// TODO Not implemented, do nothing
// 			controller->Status();
// 			break;

// 		case SCSIPowerView::CMD_SCSILINK_SETMAC:
// 			ctrl->length = 6;
// 			controller->DataOut();
// 			break;

// 		default:
// 			LOGWARN("%s Unknown SetInterface command received: %02X", __PRETTY_FUNCTION__, (unsigned int)ctrl->cmd[5]);
// 			break;
// 	}
// }

// void SCSIPowerView::SetMcastAddr(SASIDEV *controller)
// {
// 	ctrl->length = (DWORD)ctrl->cmd[4];
// 	if (ctrl->length == 0) {
// 		LOGWARN("%s Not supported SetMcastAddr Command %02X", __PRETTY_FUNCTION__, (WORD)ctrl->cmd[2]);

// 		// Failure (Error)
// 		controller->Error();
// 		return;
// 	}

// 	controller->DataOut();
// }

// void SCSIPowerView::EnableInterface(SASIDEV *controller)
// {
// 	bool status = EnableInterface(ctrl->cmd);
// 	if (!status) {
// 		// Failure (Error)
// 		controller->Error();
// 		return;
// 	}

// 	controller->Status();
// }

bool SCSIPowerView::ReceiveBuffer(int len, BYTE *buffer)
{
	int row = 0;
	int col = 0;

	for (int i = 0; i < len; i++) {
		unsigned char j = reverse_table[buffer[i]];

		for (int bit = 0; bit < 8; bit++) {
			int loc = (col * (this->fbbpp / 8)) + (row * this->fblinelen);
			col++;
			if (col % this->screen_width == 0) {
				col = 0;
				row++;
			}

			*(this->fb + loc) = (j & (1 << bit)) ? 0 : 255;
			*(this->fb + loc + 1) = (j & (1 << bit)) ? 0 : 255;
			*(this->fb + loc + 2) = (j & (1 << bit)) ? 0 : 255;
		}
	}

	return TRUE;
}