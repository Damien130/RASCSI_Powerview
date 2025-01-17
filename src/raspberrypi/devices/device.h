//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_map>
#include <string>

using namespace std;

class Device //NOSONAR The number of fields and methods is justified, the complexity is low
{
	const string DEFAULT_VENDOR = "RaSCSI";

	string type;

	bool ready = false;
	bool reset = false;
	bool attn = false;

	// Device is protectable/write-protected
	bool protectable = false;
	bool write_protected = false;
	// Device is permanently read-only
	bool read_only = false;

	// Device can be stopped (parked)/is stopped (parked)
	bool stoppable = false;
	bool stopped = false;

	// Device is removable/removed
	bool removable = false;
	bool removed = false;

	// Device is lockable/locked
	bool lockable = false;
	bool locked = false;

	// Device can be created with parameters
	bool supports_params = false;

	// Immutable LUN
	int lun;

	// Device identifier (for INQUIRY)
	string vendor = DEFAULT_VENDOR;
	string product;
	string revision;

	// The parameters the device was created with
	unordered_map<string, string> params;

	// The default parameters
	unordered_map<string, string> default_params;

	// Sense Key, ASC and ASCQ
	//	MSB		Reserved (0x00)
	//			Sense Key
	//			Additional Sense Code (ASC)
	//	LSB		Additional Sense Code Qualifier(ASCQ)
	int status_code = 0;

protected:

	void SetReady(bool b) { ready = b; }
	bool IsReset() const { return reset; }
	void SetReset(bool b) { reset = b; }
	bool IsAttn() const { return attn; }
	void SetAttn(bool b) { attn = b; }

	int GetStatusCode() const { return status_code; }

	string GetParam(const string&) const;
	void SetParams(const unordered_map<string, string>&);

	Device(const string&, int);

public:

	virtual ~Device() = default;

	const string& GetType() const { return type; }

	bool IsReady() const { return ready; }
	virtual void Reset();

	bool IsProtectable() const { return protectable; }
	void SetProtectable(bool b) { protectable = b; }
	bool IsProtected() const { return write_protected; }
	void SetProtected(bool);
	bool IsReadOnly() const { return read_only; }
	void SetReadOnly(bool b) { read_only = b; }

	bool IsStoppable() const { return stoppable; }
	void SetStoppable(bool b) { stoppable = b; }
	bool IsStopped() const { return stopped; }
	void SetStopped(bool b) { stopped = b; }
	bool IsRemovable() const { return removable; }
	void SetRemovable(bool b) { removable = b; }
	bool IsRemoved() const { return removed; }
	void SetRemoved(bool b) { removed = b; }

	bool IsLockable() const { return lockable; }
	void SetLockable(bool b) { lockable = b; }
	bool IsLocked() const { return locked; }
	void SetLocked(bool b) { locked = b; }

	virtual int GetId() const = 0;
	int GetLun() const { return lun; }

	string GetVendor() const { return vendor; }
	void SetVendor(const string&);
	string GetProduct() const { return product; }
	void SetProduct(const string&, bool = true);
	string GetRevision() const { return revision; }
	void SetRevision(const string&);
	string GetPaddedName() const;

	bool SupportsParams() const { return supports_params; }
	virtual bool SupportsFile() const { return !supports_params; }
	void SupportsParams(bool b) { supports_params = b; }
	unordered_map<string, string> GetParams() const { return params; }
	void SetDefaultParams(const unordered_map<string, string>& p) { default_params = p; }

	void SetStatusCode(int s) { status_code = s; }

	bool Start();
	void Stop();
	virtual bool Eject(bool);
};
