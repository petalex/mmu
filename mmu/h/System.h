#ifndef SYSTEM_H
#define SYSTEM_H

#include "vm_declarations.h"
#include <mutex>

class Partition;
class Process;
class KernelProcess;
class KernelSystem;

class System {
public:
	System(PhysicalAddress processVMSpace, PageNum processVMSpaceSize,
		   PhysicalAddress pmtSpace, PageNum pmtSpaceSize,
		   Partition* partition);
	~System();

	Process* createProcess();

	Process* cloneProcess(ProcessId pid);

	Time periodicJob();

	// Hardware job
	Status access(ProcessId pid, VirtualAddress address, AccessType type);
private:
	KernelSystem *pSystem;
	std::mutex mutex;
	friend class Process;
	friend class KernelProcess;
};

#endif