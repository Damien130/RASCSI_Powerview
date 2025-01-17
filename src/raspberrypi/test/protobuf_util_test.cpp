//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "rascsi_interface.pb.h"
#include "protobuf_util.h"

using namespace rascsi_interface;
using namespace protobuf_util;

void TestSpecialDevice(const string& name)
{
	PbDeviceDefinition device;
	ParseParameters(device, name);
	EXPECT_EQ(name, GetParam(device, "file"));
	EXPECT_EQ("", GetParam(device, "interfaces"));
}

TEST(CommandUtil, AddGetParam)
{
	PbCommand command;
	AddParam(command, "key", "value");
	EXPECT_EQ("value", GetParam(command, "key"));
	EXPECT_EQ("", GetParam(command, "xyz"));

	PbDeviceDefinition definition;
	AddParam(definition, "key", "value");
	EXPECT_EQ("value", GetParam(definition, "key"));
	EXPECT_EQ("", GetParam(definition, "xyz"));

	PbDevice device;
	AddParam(device, "key", "value");
	const auto& it = device.params().find("key");
	EXPECT_EQ("value", it->second);
}

TEST(CommandUtil, ParseParameters)
{
	PbDeviceDefinition device1;
	ParseParameters(device1, "a=b:c=d:e");
	EXPECT_EQ("b", GetParam(device1, "a"));
	EXPECT_EQ("d", GetParam(device1, "c"));
	EXPECT_EQ("", GetParam(device1, "e"));

	// Old style parameters
	PbDeviceDefinition device2;
	ParseParameters(device2, "a");
	EXPECT_EQ("a", GetParam(device2, "file"));
	EXPECT_EQ("a", GetParam(device2, "interfaces"));

	TestSpecialDevice("bridge");
	TestSpecialDevice("daynaport");
	TestSpecialDevice("printer");
	TestSpecialDevice("services");
}

TEST(CommandUtil, SetPatternParams)
{
	PbCommand command1;
	SetPatternParams(command1, "file");
	EXPECT_EQ("", GetParam(command1, "folder_pattern"));
	EXPECT_EQ("file", GetParam(command1, "file_pattern"));

	PbCommand command2;
	SetPatternParams(command2, ":file");
	EXPECT_EQ("", GetParam(command2, "folder_pattern"));
	EXPECT_EQ("file", GetParam(command2, "file_pattern"));

	PbCommand command3;
	SetPatternParams(command3, "folder:");
	EXPECT_EQ("folder", GetParam(command3, "folder_pattern"));
	EXPECT_EQ("", GetParam(command3, "file_pattern"));

	PbCommand command4;
	SetPatternParams(command4, "folder:file");
	EXPECT_EQ("folder", GetParam(command4, "folder_pattern"));
	EXPECT_EQ("file", GetParam(command4, "file_pattern"));
}
