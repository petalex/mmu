#include "Process.h"
#include "System.h"
#include "KernelProcess.h"
#include "KernelSystem.h"
#include <mutex>

Process::Process(ProcessId pid) {
	pProcess = new KernelProcess(this, pid);
}

Process::~Process() {
	//std::lock_guard<std::mutex> lock(this->pProcess->kernelSystem->system->mutex); // multiple deleteSegment calls
	delete pProcess;
	pProcess = nullptr;
}

ProcessId Process::getProcessId() const {
	return pProcess->getProcessId();
}

Status Process::createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags) {
	std::lock_guard<std::mutex> lock(this->pProcess->kernelSystem->system->mutex);
	return pProcess->createSegment(startAddress, segmentSize, flags);
}

Status Process::loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void* content) {
	std::lock_guard<std::mutex> lock(this->pProcess->kernelSystem->system->mutex);
	return pProcess->loadSegment(startAddress, segmentSize, flags, content);
}

Status Process::deleteSegment(VirtualAddress startAddress) {
	std::lock_guard<std::mutex> lock(this->pProcess->kernelSystem->system->mutex);
	return pProcess->deleteSegment(startAddress);
}

Process* Process::clone(ProcessId pid) {
	return pProcess->clone(pid);
}

Status Process::createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char * name, AccessType flags) {
	std::lock_guard<std::mutex> lock(this->pProcess->kernelSystem->system->mutex);
	return pProcess->createSharedSegment(startAddress, segmentSize, name, flags);
}

Status Process::disconnectSharedSegment(const char * name) {
	//std::lock_guard<std::mutex> lock(this->pProcess->kernelSystem->system->mutex);
	return pProcess->disconnectSharedSegment(name);
}

Status Process::deleteSharedSegment(const char * name) {
	std::lock_guard<std::mutex> lock(this->pProcess->kernelSystem->system->mutex);
	return pProcess->deleteSharedSegment(name);
}

Status Process::pageFault(VirtualAddress address) {
	std::lock_guard<std::mutex> lock(this->pProcess->kernelSystem->system->mutex);
	return pProcess->pageFault(address);
}

PhysicalAddress Process::getPhysicalAddress(VirtualAddress address) {
	std::lock_guard<std::mutex> lock(this->pProcess->kernelSystem->system->mutex);
	return pProcess->getPhysicalAddress(address);
}