//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//---------------------------------------------------------------------------

#pragma once

// TODO Remove this include as soon as gpiobus.cpp/h is open for editing (adding the include there) again
#include "bus.h"

namespace scsi_defs {
	enum class scsi_level : int {
		SCSI_1_CCS = 1,
		SCSI_2 = 2,
		SPC = 3,
		SPC_2 = 4,
		SPC_3 = 5,
		SPC_4 = 6,
		SPC_5 = 7,
		SPC_6 = 8
	};

	enum class device_type : int {
		DIRECT_ACCESS = 0,
		PRINTER = 2,
		PROCESSOR = 3,
		CD_ROM = 5,
		OPTICAL_MEMORY = 7,
		COMMUNICATIONS = 9
	};

	// TODO Use a mapping of enum to structure with command byte count and enum name
	enum class scsi_command : int {
		eCmdTestUnitReady = 0x00,
		eCmdRezero =  0x01,
		eCmdRequestSense = 0x03,
		eCmdFormat = 0x04,
		eCmdReassign = 0x07,
		eCmdRead6 = 0x08,
		// DaynaPort specific command
		eCmdRetrieveStats = 0x09,
		eCmdWrite6 = 0x0A,
		eCmdSeek6 = 0x0B,
		// DaynaPort specific command
		eCmdSetIfaceMode = 0x0C,
		// DaynaPort specific command
		eCmdSetMcastAddr  = 0x0D,
		// DaynaPort specific command
		eCmdEnableInterface = 0x0E,
		eCmdSynchronizeBuffer = 0x10,
		eCmdInquiry = 0x12,
		eCmdModeSelect6 = 0x15,
		eCmdReserve6 = 0x16,
		eCmdRelease6 = 0x17,
		eCmdModeSense6 = 0x1A,
		eCmdStartStop = 0x1B,
		eCmdSendDiag = 0x1D,
		eCmdRemoval = 0x1E,
		// ICD specific command
		eCmdIcd = 0x1F,
		eCmdReadCapacity10 = 0x25,
		eCmdRead10 = 0x28,
		eCmdWrite10 = 0x2A,
		eCmdSeek10 = 0x2B,
		eCmdVerify10 = 0x2F,
		eCmdSynchronizeCache10 = 0x35,
		eCmdReadDefectData10 = 0x37,
		eCmdReadLong10 = 0x3E,
		eCmdWriteLong10 = 0x3F,
		eCmdReadToc = 0x43,
		eCmdGetEventStatusNotification = 0x4A,
		eCmdModeSelect10 = 0x55,
		eCmdReserve10 = 0x56,
		eCmdRelease10 = 0x57,
		eCmdModeSense10 = 0x5A,
		eCmdRead16 = 0x88,
		eCmdWrite16 = 0x8A,
		eCmdVerify16 = 0x8F,
		eCmdSynchronizeCache16 = 0x91,
		eCmdReadCapacity16_ReadLong16 = 0x9E,
		eCmdWriteLong16 = 0x9F,
		eCmdReportLuns = 0xA0
	};

	enum class status : int {
		GOOD = 0x00,
		CHECK_CONDITION = 0x02,
		CONDITION_MET = 0x04,
		BUSY = 0x08,
		INTERMEDIATE = 0x10,
		INTERMEDIATE_CONDITION_MET = 0x14,
		RESERVATION_CONFLICT = 0x18,
		COMMAND_TERMINATED = 0x22,
		QUEUE_FULL = 0x28
	};

	enum class sense_key : int {
		NO_SENSE = 0x00,
		RECOVERED_ERROR = 0x01,
		NOT_READY = 0x02,
		MEDIUM_ERROR = 0x03,
		HARDWARE_ERROR = 0x04,
		ILLEGAL_REQUEST = 0x05,
		UNIT_ATTENTION = 0x06,
		DATA_PROTECT = 0x07,
		BLANK_CHECK = 0x08,
		VENDOR_SPECIFIC = 0x09,
		COPY_ABORTED = 0x0a,
		ABORTED_COMMAND = 0x0b,
		VOLUME_OVERFLOW = 0x0d,
		MISCOMPARE = 0x0e,
		COMPLETED = 0x0f
	};

	enum class asc : int {
		NO_ADDITIONAL_SENSE_INFORMATION = 0x00,
		WRITE_FAULT = 0x03,
		READ_FAULT = 0x11,
		INVALID_COMMAND_OPERATION_CODE = 0x20,
		LBA_OUT_OF_RANGE = 0x21,
		INVALID_FIELD_IN_CDB = 0x24,
		INVALID_LUN = 0x25,
		INVALID_FIELD_IN_PARAMETER_LIST = 0x26,
		WRITE_PROTECTED = 0x27,
		NOT_READY_TO_READY_CHANGE = 0x28,
		POWER_ON_OR_RESET = 0x29,
		MEDIUM_NOT_PRESENT = 0x3a,
		LOAD_OR_EJECT_FAILED = 0x53
	};
};
