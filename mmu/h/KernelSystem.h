#ifndef KERNEL_SYSTEM_H
#define KERNEL_SYSTEM_H

#include "vm_declarations.h"
#include "part.h"
#include <unordered_map>

const Time nextTime = 10000;

class Partition;
class Process;
class System;
class KernelProcess;

class Allocator;
class ProcessList;
class DescriptorList;
class SharedSegmentDescriptor;

class KernelSystem {
public:
	KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize,
		PhysicalAddress pmtSpace, PageNum pmtSpaceSize,
		Partition* partition, System *system);
	~KernelSystem();

	Process* createProcess();

	Process* cloneProcess(ProcessId pid);

	Time periodicJob();

	Status access(ProcessId pid, VirtualAddress address, AccessType type);
protected:
	PageNum allocateBlock();
	void deallocateBlock(PageNum block);
	PhysicalAddress allocatePMT();
	void deallocatePMT(PhysicalAddress pmt);
	ClusterNo allocateCluster();
	void deallocateCluster(ClusterNo cluster);
	void deleteProcess(ProcessId pid);
	PageNum getVictim();

	bool checkSharedSegment(const char* name) { 
		std::unordered_map<const char *, SharedSegmentDescriptor *>::const_iterator it = descriptors.find(name);
		return it != descriptors.end() && it->second != nullptr;
	}
	void createSharedSegment(const char* name, PageNum segmentSize, unsigned RWX);
private:
	friend class Process;
	friend class KernelProcess;
	friend class SharedSegmentDescriptor;

	PhysicalAddress processVMSpace, pmtSpace;
	Partition *partition;
	Allocator *vmAllocator, *pmtAllocator, *partAllocator;
	ProcessList *processList;
	DescriptorList *validDescriptorList;
	System *system;

	static ProcessId pidGenerator;

	std::unordered_map <const char *, SharedSegmentDescriptor *> descriptors;
};

#endif
