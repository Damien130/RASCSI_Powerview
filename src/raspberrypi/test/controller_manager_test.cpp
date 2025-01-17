//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/device_factory.h"
#include "controllers/controller_manager.h"

TEST(ControllerManagerTest, LifeCycle)
{
	const int ID = 4;
	const int LUN1 = 2;
	const int LUN2 = 3;

	MockBus bus;
	ControllerManager controller_manager(bus);
	DeviceFactory device_factory;

	auto device = device_factory.CreateDevice(controller_manager, UNDEFINED, -1, "services");
	EXPECT_FALSE(controller_manager.AttachToScsiController(ID, device));

	device = device_factory.CreateDevice(controller_manager, UNDEFINED, LUN1, "services");
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device));
	auto controller = controller_manager.FindController(ID);
	EXPECT_NE(nullptr, controller);
	EXPECT_EQ(1, controller->GetLunCount());
	EXPECT_NE(nullptr, controller_manager.IdentifyController(1 << ID));
	EXPECT_EQ(nullptr, controller_manager.IdentifyController(0));
	EXPECT_EQ(nullptr, controller_manager.FindController(0));
	EXPECT_NE(nullptr, controller_manager.GetDeviceByIdAndLun(ID, LUN1));
	EXPECT_EQ(nullptr, controller_manager.GetDeviceByIdAndLun(0, 0));

	device = device_factory.CreateDevice(controller_manager, UNDEFINED, LUN2, "services");
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device));
	controller = controller_manager.FindController(ID);
	EXPECT_TRUE(controller_manager.DeleteController(controller));
	EXPECT_EQ(nullptr, controller_manager.FindController(ID));

	controller_manager.DeleteAllControllers();
	EXPECT_EQ(nullptr, controller_manager.FindController(ID));
	EXPECT_EQ(nullptr, controller_manager.GetDeviceByIdAndLun(ID, LUN1));
}

TEST(ControllerManagerTest, ResetAllControllers)
{
	const int ID = 2;

	MockBus bus;
	ControllerManager controller_manager(bus);
	DeviceFactory device_factory;

	auto device = device_factory.CreateDevice(controller_manager, UNDEFINED, 0, "services");
	EXPECT_TRUE(controller_manager.AttachToScsiController(ID, device));
	auto controller = controller_manager.FindController(ID);
	EXPECT_NE(nullptr, controller);

	controller->SetStatus(status::BUSY);
	controller_manager.ResetAllControllers();
	EXPECT_EQ(status::GOOD, controller->GetStatus());
}
