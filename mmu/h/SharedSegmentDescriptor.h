#ifndef SHARED_SEGMENT_DESCRIPTOR_H
#define SHARED_SEGMENT_DESCRIPTOR_H

#include "part.h"
#include "ProcessList.h"
#include "Allocator.h"
#include <mutex>

class KernelSystem;
class Process;
struct Descriptor;

typedef struct processVANode {
	Process *process;
	VirtualAddress startAddress;
	struct processVANode *next;
	processVANode(Process *process, VirtualAddress startAddress, struct processVANode* next = nullptr) : process(process), startAddress(startAddress), next(next) {}
} ProcessVANode;

class SharedSegmentDescriptor {
public:
	SharedSegmentDescriptor::SharedSegmentDescriptor(KernelSystem *kernelSystem, PageNum segmentSize, unsigned RWX);
	~SharedSegmentDescriptor();
	PageNum getSize() { return this->segmentSize; }
	unsigned getRWX() { return this->RWX; }
	bool isConnected(Process *process, VirtualAddress address);
	bool isLast(Process *process);
	VirtualAddress getOffset(Process *process, VirtualAddress address);
	void connectProcess(Process *process, VirtualAddress startAddress);
	void disconnectProcess(Process *process);
	void disconnectAllProcesses();
	void updateAllDescriptors(VirtualAddress offset, PageNum block);
	void setSharedDescriptor(Descriptor *descriptor, VirtualAddress offset);
	VirtualAddress getStartAddress(Process *process);
	ClusterNo getCluster(unsigned n);
private:
	KernelSystem *kernelSystem;
	ProcessVANode *firstProcess, *lastProcess;
	BlockNode *firstCluster, *lastCluster;
	PageNum segmentSize;
	unsigned RWX : 3;
	std::mutex mutex;
};

#endif