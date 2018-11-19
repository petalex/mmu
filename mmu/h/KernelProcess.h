#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include "vm_declarations.h"
#include "part.h"
#define PAGE1 8
#define PAGE2 6
#define WORD 10
#define RD 4
#define WR 2
#define RD_WR 6
#define EX 1

const VirtualAddress validVaMask = ((1 << (sizeof(VirtualAddress)*8 - (PAGE1 + PAGE2 + WORD))) - 1) << (PAGE1 + PAGE2 + WORD);
const VirtualAddress page1Mask = ((1 << PAGE1) - 1) << (PAGE2 + WORD);
const VirtualAddress page2Mask = ((1 << PAGE2) - 1) << WORD;
const VirtualAddress wordMask = (1 << WORD) - 1;
const unsigned pmt1Size = 1 << PAGE1;
const unsigned pmt2Size = 1 << PAGE2;

class KernelSystem;
class Process;

typedef struct Descriptor {
	unsigned V : 1;
	unsigned D : 1;
	unsigned RWX : 3;
	unsigned SEGMENT : 1;
	unsigned SEGMENT_START : 1;
	unsigned SEGMENT_SHARED : 1;
	unsigned REF : 1;
	unsigned REF_HISTORY;
	PageNum block;
	ClusterNo disk;
} Descriptor;

typedef struct PMT2 {
	Descriptor descriptors[pmt2Size];
} PMT2;

typedef struct PMT1 {
	PMT2 *pointers[pmt1Size];
} PMT1;

class KernelProcess {
public:
	KernelProcess(Process *process, ProcessId pid): process(process), pid(pid), kernelSystem(nullptr), pmt(nullptr) {}
	void setKernelSystem(KernelSystem *kernelSystem) {	this->kernelSystem = kernelSystem;	}
	void setPMT1();
	~KernelProcess();
	ProcessId getProcessId() const;

	Status createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags);
	Status loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void* content);
	Status deleteSegment(VirtualAddress startAddress);

	Process* clone(ProcessId pid);
	Status createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char* name, AccessType flags);
	Status disconnectSharedSegment(const char* name);
	Status deleteSharedSegment(const char* name);

	Status pageFault(VirtualAddress address);
	PhysicalAddress getPhysicalAddress(VirtualAddress address);

protected:
	Status access(VirtualAddress address, AccessType type);
	unsigned accessType2RWX(AccessType flags);
	AccessType rwx2AccessType(unsigned RWX);

	const char* getSharedSegmentName(VirtualAddress address);
private:
	friend class KernelSystem;
	friend class Process;

	friend class SharedSegmentDescriptor;

	Process *process;
	ProcessId pid;
	KernelSystem *kernelSystem;
	PMT1 *pmt;
};

#endif