#include "System.h"
#include "KernelSystem.h"

System::System(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition* partition) {
	std::lock_guard<std::mutex> lock(mutex);
	pSystem = new KernelSystem(processVMSpace, processVMSpaceSize, pmtSpace, pmtSpaceSize, partition, this);
}

System::~System() {
	std::lock_guard<std::mutex> lock(mutex);
	delete pSystem;
	pSystem = nullptr;
}

Process* System::createProcess() {
	std::lock_guard<std::mutex> lock(mutex);
	return pSystem->createProcess();
}

Process* System::cloneProcess(ProcessId pid) {
	std::lock_guard<std::mutex> lock(mutex);
	return pSystem->cloneProcess(pid);
}

Time System::periodicJob() {
	std::lock_guard<std::mutex> lock(mutex);
	return pSystem->periodicJob();
}

Status System::access(ProcessId pid, VirtualAddress address, AccessType type) {
	std::lock_guard<std::mutex> lock(mutex);
	return pSystem->access(pid, address, type);
}