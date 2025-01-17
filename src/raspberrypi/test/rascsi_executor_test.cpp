//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "protobuf_util.h"
#include "controllers/controller_manager.h"
#include "devices/device_factory.h"
#include "rascsi/command_context.h"
#include "rascsi/rascsi_response.h"
#include "rascsi/rascsi_image.h"
#include "rascsi/rascsi_executor.h"

using namespace rascsi_interface;
using namespace protobuf_util;

TEST(RascsiExecutorTest, ProcessCmd)
{
	const int ID = 3;
	const int LUN = 0;

	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	PbDeviceDefinition definition;
	PbCommand command;
	MockCommandContext context;

	definition.set_id(8);
	definition.set_unit(32);
	EXPECT_FALSE(executor.ProcessCmd(context, definition, command, true)) << "Invalid ID must fail";

	definition.set_id(ID);
	EXPECT_FALSE(executor.ProcessCmd(context, definition, command, true)) << "Invalid LUN must fail";

	definition.set_unit(LUN);
	EXPECT_FALSE(executor.ProcessCmd(context, definition, command, true)) << "Unknown operation must fail";

	command.set_operation(ATTACH);
	EXPECT_FALSE(executor.ProcessCmd(context, definition, command, true)) << "Operation for unknown device must fail";
}

TEST(RascsiExecutorTest, SetLogLevel)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);

	EXPECT_TRUE(executor.SetLogLevel("trace"));
	EXPECT_TRUE(executor.SetLogLevel("debug"));
	EXPECT_TRUE(executor.SetLogLevel("info"));
	EXPECT_TRUE(executor.SetLogLevel("warn"));
	EXPECT_TRUE(executor.SetLogLevel("err"));
	EXPECT_TRUE(executor.SetLogLevel("critical"));
	EXPECT_TRUE(executor.SetLogLevel("off"));
	EXPECT_FALSE(executor.SetLogLevel("xyz"));
}

TEST(RascsiExecutorTest, Attach)
{
	const int ID = 3;
	const int LUN = 0;

	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	PbDeviceDefinition definition;
	MockCommandContext context;

	definition.set_unit(32);
	EXPECT_FALSE(executor.Attach(context, definition, false));

	auto device = device_factory.CreateDevice(controller_manager, SCHD, LUN, "");
	definition.set_id(ID);
	definition.set_unit(LUN);

	executor.SetReservedIds(to_string(ID));
	EXPECT_FALSE(executor.Attach(context, definition, false)) << "Reserved ID not rejected";

	executor.SetReservedIds("");
	EXPECT_FALSE(executor.Attach(context, definition, false)) << "Unknown device type not rejected";

	definition.set_type(PbDeviceType::SCHS);
	EXPECT_TRUE(executor.Attach(context, definition, false));
	controller_manager.DeleteAllControllers();

	definition.set_type(PbDeviceType::SCHD);
	EXPECT_FALSE(executor.Attach(context, definition, false)) << "Drive without sectors not rejected";

	definition.set_block_size(1);
	EXPECT_FALSE(executor.Attach(context, definition, false)) << "Drive with invalid sector size not rejeced";

	definition.set_block_size(1024);
	EXPECT_FALSE(executor.Attach(context, definition, false)) << "Drive without image file not rejected";

	AddParam(definition, "file", "/non_existing_file");
	EXPECT_FALSE(executor.Attach(context, definition, false));

	AddParam(definition, "file", "/dev/zero");
	EXPECT_FALSE(executor.Attach(context, definition, false)) << "Empty image file not rejected";

	// Further testing is not possible without a filesystem
}

TEST(RascsiExecutorTest, Insert)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	PbDeviceDefinition definition;
	MockCommandContext context;

	auto device = device_factory.CreateDevice(controller_manager, SCRM, 0, "test");

	device->SetRemoved(false);
	EXPECT_FALSE(executor.Insert(context, definition, device, false)) << "Medium is not removed";

	device->SetRemoved(true);
	definition.set_vendor("v");
	EXPECT_FALSE(executor.Insert(context, definition, device, false)) << "Product data must not be set";
	definition.set_vendor("");

	definition.set_product("p");
	EXPECT_FALSE(executor.Insert(context, definition, device, false)) << "Product data must not be set";
	definition.set_product("");

	definition.set_revision("r");
	EXPECT_FALSE(executor.Insert(context, definition, device, false)) << "Product data must not be set";
	definition.set_revision("");

	EXPECT_FALSE(executor.Insert(context, definition, device, false)) << "Filename is missing";

	AddParam(definition, "file", "filename");
	EXPECT_TRUE(executor.Insert(context, definition, device, true)) << "Dry-run must not fail";
	EXPECT_FALSE(executor.Insert(context, definition, device, false));

	definition.set_block_size(1);
	EXPECT_FALSE(executor.Insert(context, definition, device, false));

	definition.set_block_size(0);
	EXPECT_FALSE(executor.Insert(context, definition, device, false)) << "Image file validation has to fail";

	AddParam(definition, "file", "/non_existing_file");
	EXPECT_FALSE(executor.Insert(context, definition, device, false));

	AddParam(definition, "file", "/dev/zero");
	EXPECT_FALSE(executor.Insert(context, definition, device, false)) << "File has 0 bytes";

	// Further testing is not possible without a filesystem
}

TEST(RascsiExecutorTest, Detach)
{
	const int ID = 3;
	const int LUN1 = 0;
	const int LUN2 = 1;

	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	auto device1 = device_factory.CreateDevice(controller_manager, UNDEFINED, LUN1, "services");
	controller_manager.AttachToScsiController(ID, device1);
	auto device2 = device_factory.CreateDevice(controller_manager, UNDEFINED, LUN2, "services");
	controller_manager.AttachToScsiController(ID, device2);

	auto d1 = controller_manager.GetDeviceByIdAndLun(ID, LUN1);
	EXPECT_FALSE(executor.Detach(context, d1, false)) << "LUNs > 0 have to be detached first";
	auto d2 = controller_manager.GetDeviceByIdAndLun(ID, LUN2);
	EXPECT_TRUE(executor.Detach(context, d2, false));
	EXPECT_TRUE(executor.Detach(context, d1, false));
	EXPECT_TRUE(controller_manager.GetAllDevices().empty());
}

TEST(RascsiExecutorTest, DetachAll)
{
	const int ID = 4;

	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);

	auto device = device_factory.CreateDevice(controller_manager, UNDEFINED, 0, "services");
	controller_manager.AttachToScsiController(ID, device);
	EXPECT_NE(nullptr, controller_manager.FindController(ID));
	EXPECT_FALSE(controller_manager.GetAllDevices().empty());

	executor.DetachAll();
	EXPECT_EQ(nullptr, controller_manager.FindController(ID));
	EXPECT_TRUE(controller_manager.GetAllDevices().empty());
}

TEST(RascsiExecutorTest, ShutDown)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	EXPECT_FALSE(executor.ShutDown(context, ""));
	EXPECT_FALSE(executor.ShutDown(context, "xyz"));
	EXPECT_TRUE(executor.ShutDown(context, "rascsi"));
	EXPECT_FALSE(executor.ShutDown(context, "system"));
	EXPECT_FALSE(executor.ShutDown(context, "reboot"));
}

TEST(RascsiExecutorTest, SetReservedIds)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);

	string error = executor.SetReservedIds("xyz");
	EXPECT_FALSE(error.empty());
	EXPECT_TRUE(executor.GetReservedIds().empty());

	error = executor.SetReservedIds("8");
	EXPECT_FALSE(error.empty());
	EXPECT_TRUE(executor.GetReservedIds().empty());

	error = executor.SetReservedIds("-1");
	EXPECT_FALSE(error.empty());
	EXPECT_TRUE(executor.GetReservedIds().empty());

	error = executor.SetReservedIds("");
	EXPECT_TRUE(error.empty());
	EXPECT_TRUE(executor.GetReservedIds().empty());

	error = executor.SetReservedIds("7,1,2,3,5");
	EXPECT_TRUE(error.empty());
	unordered_set<int> reserved_ids = executor.GetReservedIds();
	EXPECT_EQ(5, reserved_ids.size());
	EXPECT_NE(reserved_ids.end(), reserved_ids.find(1));
	EXPECT_NE(reserved_ids.end(), reserved_ids.find(2));
	EXPECT_NE(reserved_ids.end(), reserved_ids.find(3));
	EXPECT_NE(reserved_ids.end(), reserved_ids.find(5));
	EXPECT_NE(reserved_ids.end(), reserved_ids.find(7));

	auto device = device_factory.CreateDevice(controller_manager, UNDEFINED, 0, "services");
	controller_manager.AttachToScsiController(5, device);
	error = executor.SetReservedIds("5");
	EXPECT_FALSE(error.empty());
}

TEST(RascsiExecutorTest, ValidateImageFile)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	string full_path;
	auto device = device_factory.CreateDevice(controller_manager, UNDEFINED, 0, "services");
	EXPECT_TRUE(executor.ValidateImageFile(context, device, "", full_path));
	EXPECT_TRUE(full_path.empty());

	device = device_factory.CreateDevice(controller_manager, SCHD, 0, "test");
	EXPECT_TRUE(executor.ValidateImageFile(context, device, "", full_path));
	EXPECT_TRUE(full_path.empty());

	EXPECT_FALSE(executor.ValidateImageFile(context, device, "/non_existing_file", full_path));
	EXPECT_TRUE(full_path.empty());
}

TEST(RascsiExecutorTest, ValidateLunSetup)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	PbCommand command;

	auto device1 = command.add_devices();
	device1->set_unit(0);
	string error = executor.ValidateLunSetup(command);
	EXPECT_TRUE(error.empty());

	device1->set_unit(1);
	error = executor.ValidateLunSetup(command);
	EXPECT_FALSE(error.empty());

	auto device2 = device_factory.CreateDevice(controller_manager, UNDEFINED, 0, "services");
	controller_manager.AttachToScsiController(0, device2);
	error = executor.ValidateLunSetup(command);
	EXPECT_TRUE(error.empty());
}

TEST(RascsiExecutorTest, VerifyExistingIdAndLun)
{
	const int ID = 1;
	const int LUN1 = 2;
	const int LUN2 = 3;

	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	EXPECT_FALSE(executor.VerifyExistingIdAndLun(context, ID, LUN1));
	auto device = device_factory.CreateDevice(controller_manager, UNDEFINED, LUN1, "services");
	controller_manager.AttachToScsiController(ID, device);
	EXPECT_TRUE(executor.VerifyExistingIdAndLun(context, ID, LUN1));
	EXPECT_FALSE(executor.VerifyExistingIdAndLun(context, ID, LUN2));
}

TEST(RascsiExecutorTest, CreateDevice)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	EXPECT_EQ(nullptr, executor.CreateDevice(context, UNDEFINED, 0, ""));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	EXPECT_EQ(nullptr, executor.CreateDevice(context, SAHD, 0, ""));
#pragma GCC diagnostic pop
	EXPECT_NE(nullptr, executor.CreateDevice(context, UNDEFINED, 0, "services"));
	EXPECT_NE(nullptr, executor.CreateDevice(context, SCHS, 0, ""));
}

TEST(RascsiExecutorTest, SetSectorSize)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	unordered_set<uint32_t> sizes;
	auto disk = make_shared<MockSCSIHD>(0, sizes, false);
	EXPECT_FALSE(executor.SetSectorSize(context, "test", disk, 512));

	sizes.insert(512);
	disk = make_shared<MockSCSIHD>(0, sizes, false);
	EXPECT_TRUE(executor.SetSectorSize(context, "test", disk, 0));
	EXPECT_FALSE(executor.SetSectorSize(context, "test", disk, 1));
	EXPECT_TRUE(executor.SetSectorSize(context, "test", disk, 512));
}

TEST(RascsiExecutorTest, ValidationOperationAgainstDevice)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	auto device = make_shared<MockPrimaryDevice>(0);

	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, ATTACH));
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, DETACH));

	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, START));
	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, STOP));
	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, INSERT));
	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, EJECT));
	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, PROTECT));
	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, UNPROTECT));

	device->SetStoppable(true);
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, START));
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, STOP));
	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, INSERT));
	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, EJECT));
	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, PROTECT));
	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, UNPROTECT));

	device->SetRemovable(true);
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, START));
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, STOP));
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, INSERT));
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, EJECT));
	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, PROTECT));
	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, UNPROTECT));

	device->SetProtectable(true);
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, START));
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, STOP));
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, INSERT));
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, EJECT));
	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, PROTECT));
	EXPECT_FALSE(executor.ValidationOperationAgainstDevice(context, device, UNPROTECT));

	device->SetReady(true);
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, START));
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, STOP));
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, INSERT));
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, EJECT));
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, PROTECT));
	EXPECT_TRUE(executor.ValidationOperationAgainstDevice(context, device, UNPROTECT));
}

TEST(RascsiExecutorTest, ValidateIdAndLun)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;

	EXPECT_FALSE(executor.ValidateIdAndLun(context, -1, 0));
	EXPECT_FALSE(executor.ValidateIdAndLun(context, 8, 0));
	EXPECT_FALSE(executor.ValidateIdAndLun(context, 7, -1));
	EXPECT_FALSE(executor.ValidateIdAndLun(context, 7, 32));
	EXPECT_TRUE(executor.ValidateIdAndLun(context, 7, 0));
	EXPECT_TRUE(executor.ValidateIdAndLun(context, 7, 31));
}

TEST(RascsiExecutorTest, SetProductData)
{
	MockBus bus;
	DeviceFactory device_factory;
	ControllerManager controller_manager(bus);
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(device_factory, controller_manager, 32);
	RascsiExecutor executor(rascsi_response, rascsi_image, device_factory, controller_manager);
	MockCommandContext context;
	PbDeviceDefinition definition;

	auto device = make_shared<MockPrimaryDevice>(0);

	EXPECT_TRUE(executor.SetProductData(context, definition, device));

	definition.set_vendor("123456789");
	EXPECT_FALSE(executor.SetProductData(context, definition, device));
	definition.set_vendor("1");
	EXPECT_TRUE(executor.SetProductData(context, definition, device));
	definition.set_vendor("12345678");
	EXPECT_TRUE(executor.SetProductData(context, definition, device));

	definition.set_product("12345678901234567");
	EXPECT_FALSE(executor.SetProductData(context, definition, device));
	definition.set_product("1");
	EXPECT_TRUE(executor.SetProductData(context, definition, device));
	definition.set_product("1234567890123456");
	EXPECT_TRUE(executor.SetProductData(context, definition, device));

	definition.set_revision("12345");
	EXPECT_FALSE(executor.SetProductData(context, definition, device));
	definition.set_revision("1");
	EXPECT_TRUE(executor.SetProductData(context, definition, device));
	definition.set_revision("1234");
	EXPECT_TRUE(executor.SetProductData(context, definition, device));
}
