#include "SharedSegmentDescriptor.h"
#include "KernelSystem.h"
#include "KernelProcess.h"
#include "Process.h"
#include "DescriptorList.h"

SharedSegmentDescriptor::SharedSegmentDescriptor(KernelSystem *kernelSystem, PageNum segmentSize, unsigned RWX)
	: kernelSystem(kernelSystem), firstProcess(nullptr), lastProcess(nullptr), firstCluster(nullptr), lastCluster(nullptr), segmentSize(segmentSize), RWX(RWX) {
	for (unsigned i = 0; i < this->segmentSize; i++) {
		this->lastCluster = ((this->firstCluster != nullptr) ? this->lastCluster->next : this->firstCluster) = new BlockNode(this->kernelSystem->allocateCluster());
	}
}

SharedSegmentDescriptor::~SharedSegmentDescriptor() {
	this->disconnectAllProcesses();
	//std::lock_guard<std::mutex> lock(mutex);
	while (this->firstCluster != nullptr) {
		BlockNode *temp = this->firstCluster;
		this->firstCluster = this->firstCluster->next;
		this->kernelSystem->deallocateCluster(temp->block);
		delete temp;
	}
	this->lastCluster = nullptr;
	this->kernelSystem = nullptr;
}

bool SharedSegmentDescriptor::isConnected(Process * process, VirtualAddress address) {
	//std::lock_guard<std::mutex> lock(mutex);
	ProcessVANode  *prev = nullptr, *cur = this->firstProcess;
	while (cur != nullptr) {
		VirtualAddress endAddress = cur->startAddress + ((this->segmentSize - 1) << WORD);
		if (cur->process == process)
			if (cur->startAddress <= address && address <= endAddress) return true;
			else return false;
		cur = cur->next;
	}

	return false;
}

bool SharedSegmentDescriptor::isLast(Process * process) {
	//std::lock_guard<std::mutex> lock(mutex);
	return this->firstProcess->process == process && this->firstProcess->next == nullptr;
}

VirtualAddress SharedSegmentDescriptor::getOffset(Process * process, VirtualAddress address) {
	//std::lock_guard<std::mutex> lock(mutex);
	ProcessVANode  *prev = nullptr, *cur = this->firstProcess;
	while (cur != nullptr) {
		if (cur->process == process) return address - cur->startAddress;
		cur = cur->next;
	}
}

void SharedSegmentDescriptor::connectProcess(Process * process, VirtualAddress startAddress) {
	//std::lock_guard<std::mutex> lock(mutex);
	this->lastProcess = ((this->firstProcess != nullptr) ? this->lastProcess->next : this->firstProcess) = new ProcessVANode(process, startAddress);
}

void SharedSegmentDescriptor::disconnectProcess(Process * process) {
	//std::lock_guard<std::mutex> lock(mutex);
	ProcessVANode  *prev = nullptr, *cur = this->firstProcess;
	while (cur != nullptr && cur->process != process) {
		prev = cur;
		cur = cur->next;
	}
	if (cur == nullptr) return;
	if (prev != nullptr) {
		prev->next = cur->next;
		if (cur == this->lastProcess)
			 this->lastProcess = prev;
	}
	else this->firstProcess = cur->next;
	if (this->firstProcess == nullptr) this->lastProcess = nullptr;
	delete cur;
}

void SharedSegmentDescriptor::disconnectAllProcesses() {
	//std::lock_guard<std::mutex> lock(mutex);
	ProcessVANode  *cur = this->firstProcess;
	while (cur != nullptr) {
		ProcessVANode  *temp = cur;
		cur = cur->next;
		temp->process->disconnectSharedSegment(temp->process->pProcess->getSharedSegmentName(temp->startAddress));
	}
}

void SharedSegmentDescriptor::updateAllDescriptors(VirtualAddress offset, PageNum block) {
	//std::lock_guard<std::mutex> lock(mutex);
	ProcessVANode  *cur = this->firstProcess;
	unsigned refBitMask = 0x80000000;
	while (cur != nullptr) {
		VirtualAddress address = cur->startAddress + offset;
		unsigned page1 = (address & page1Mask) >> (PAGE2 + WORD);
		unsigned page2 = (address & page2Mask) >> WORD;
		Descriptor *descriptor = &(cur->process->pProcess->pmt->pointers[page1]->descriptors[page2]);
		// Set page's descriptor
		descriptor->V = 1;
		descriptor->D = 0;
		descriptor->REF = 0;
		descriptor->REF_HISTORY = refBitMask;
		descriptor->block = block;
		// Update kernel system
		this->kernelSystem->validDescriptorList->putDescriptor(descriptor);
		cur = cur->next;
	}
}

void SharedSegmentDescriptor::setSharedDescriptor(Descriptor * descriptor, VirtualAddress offset) {
	//std::lock_guard<std::mutex> lock(mutex);
	if (this->firstProcess != nullptr) {
		// Copy descriptor from some connected process
		VirtualAddress address = this->firstProcess->startAddress + offset;
		unsigned page1 = (address & page1Mask) >> (PAGE2 + WORD);
		unsigned page2 = (address & page2Mask) >> WORD;
		Descriptor *oldDescriptor = &(this->firstProcess->process->pProcess->pmt->pointers[page1]->descriptors[page2]);
		descriptor->V = oldDescriptor->V;
		descriptor->D = oldDescriptor->D;
		descriptor->RWX = oldDescriptor->RWX;
		descriptor->SEGMENT = oldDescriptor->SEGMENT;
		descriptor->SEGMENT_START = oldDescriptor->SEGMENT_START;
		descriptor->SEGMENT_SHARED = oldDescriptor->SEGMENT_SHARED;
		descriptor->REF = oldDescriptor->REF;
		descriptor->REF_HISTORY = oldDescriptor->REF_HISTORY;
		descriptor->block = oldDescriptor->block;
		descriptor->disk = oldDescriptor->disk;
		// Update kernel system
		if (descriptor->V) this->kernelSystem->validDescriptorList->putDescriptor(descriptor);
	}
	else {
		// Empty, on creation or when all have been disconnected, but segment not deleted
		descriptor->V = 0;
		descriptor->D = 0;
		descriptor->RWX = this->RWX;
		descriptor->SEGMENT = 1;
		descriptor->SEGMENT_START = ((offset >> WORD) == 0) ? 1 : 0;
		descriptor->SEGMENT_SHARED = 1;
		descriptor->REF = 0;
		descriptor->REF_HISTORY = 0;
		descriptor->block = 0; // will be changed after first page fault
		descriptor->disk = this->getCluster(offset >> WORD);
	}
}

VirtualAddress SharedSegmentDescriptor::getStartAddress(Process * process) {
	//std::lock_guard<std::mutex> lock(mutex);
	ProcessVANode  *cur = this->firstProcess;
	while (cur != nullptr) {
		if (cur->process == process) return cur->startAddress;
		cur = cur->next;
	}

	return 0;
}

ClusterNo SharedSegmentDescriptor::getCluster(unsigned n) {
	//std::lock_guard<std::mutex> lock(mutex);
	if (n < 0 || n >= this->segmentSize) 
		return -1;
	BlockNode *cur = this->firstCluster;
	for (unsigned i = 0; i < n; i++) {
		cur = cur->next;
	}
	return cur->block;
}
