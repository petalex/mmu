#include "Process.h"
#include "KernelProcess.h"
#include "KernelSystem.h"
#include "DescriptorList.h"
#include "Allocator.h"
#include "ProcessList.h"
#include "SharedSegmentDescriptor.h"

void KernelProcess::setPMT1() {
	this->pmt = (PMT1 *)this->kernelSystem->allocatePMT();
	for (unsigned i = 0; i < pmt1Size; this->pmt->pointers[i++] = 0);
}

KernelProcess::~KernelProcess() {
	bool checkMore;
	do {
		checkMore = false;
		for (unsigned i = 0; i < pmt1Size; i++) {
			if (this->pmt->pointers[i] != nullptr) {
				for (unsigned j = 0; j < pmt2Size; j++) {
					if (this->pmt->pointers[i]->descriptors[j].SEGMENT == 1 && this->pmt->pointers[i]->descriptors[j].SEGMENT_START == 1) {
						VirtualAddress segmentStartAddress = ((i << PAGE2) + j) << WORD;
						if (this->pmt->pointers[i]->descriptors[j].SEGMENT_SHARED == 1)
							this->disconnectSharedSegment(this->getSharedSegmentName(segmentStartAddress));
						else
							this->deleteSegment(segmentStartAddress);
						checkMore = true;
						break;
					}
				}
			}
			if (checkMore) break;
		}
	} while (checkMore);
	this->kernelSystem->deallocatePMT((PhysicalAddress)this->pmt);
	this->pmt = nullptr;
	this->kernelSystem->processList->removeProcess(this->pid);
	this->kernelSystem = nullptr;
	this->process->pProcess = nullptr;
	this->process = nullptr;
}

ProcessId KernelProcess::getProcessId() const {
	return this->pid;
}

Status KernelProcess::createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags) {
	// Check if virtual address is valid and on page's beginning
	if (startAddress & validVaMask) return TRAP;
	if (startAddress & wordMask) return TRAP;
	VirtualAddress endAddress = startAddress + ((segmentSize - 1) << WORD);
	// Check if end virtual address is valid
	if (endAddress & validVaMask) return TRAP;
	unsigned startPage1 = (startAddress & page1Mask) >> (PAGE2 + WORD);
	unsigned endPage1 = (endAddress & page1Mask) >> (PAGE2 + WORD);
	unsigned counter = 0;
	for (unsigned i = startPage1; i <= endPage1; i++) {
		if (this->pmt->pointers[i] == 0) counter++; // count all pmt entries that have to be allocated
	}
	if (counter > 0 && this->kernelSystem->pmtAllocator->noSpace(counter)) return TRAP;
	if (this->kernelSystem->partAllocator->noSpace(segmentSize)) return TRAP;
	for (unsigned i = startPage1; i <= endPage1; i++) {
		if (this->pmt->pointers[i] == 0) {
			// Allocate all pmt2 entries
			this->pmt->pointers[i] = (PMT2 *)this->kernelSystem->allocatePMT();
			for (unsigned j = 0; j < pmt2Size; this->pmt->pointers[i]->descriptors[j++].SEGMENT = 0);
		}
		else {
			// Check all pmt2 entries if already in another segment
			unsigned startPage2 = (i == startPage1) ? ((startAddress & page2Mask)) >> WORD : 0;
			unsigned endPage2 = (i == endPage1) ? ((endAddress & page2Mask) >> WORD) : (pmt2Size - 1);
			for (unsigned j = startPage2; j <= endPage2; j++)
				if (this->pmt->pointers[i]->descriptors[j].SEGMENT == 1) return TRAP;
		}
	}
	// Set segment's descriptors
	for (unsigned i = startPage1; i <= endPage1; i++) {
		unsigned startPage2 = (i == startPage1) ? ((startAddress & page2Mask) >> WORD) : 0;
		unsigned endPage2 = (i == endPage1) ? ((endAddress & page2Mask) >> WORD) : (pmt2Size - 1);
		for (unsigned j = startPage2; j <= endPage2; j++) {
			this->pmt->pointers[i]->descriptors[j].V = 0;
			this->pmt->pointers[i]->descriptors[j].D = 0;
			this->pmt->pointers[i]->descriptors[j].RWX = this->accessType2RWX(flags);
			this->pmt->pointers[i]->descriptors[j].SEGMENT = 1;
			this->pmt->pointers[i]->descriptors[j].SEGMENT_START = (i == startPage1 && j == startPage2) ? 1 : 0;
			this->pmt->pointers[i]->descriptors[j].SEGMENT_SHARED = 0;
			this->pmt->pointers[i]->descriptors[j].REF = 0;
			this->pmt->pointers[i]->descriptors[j].REF_HISTORY = 0;
			this->pmt->pointers[i]->descriptors[j].block = 0; // will be changed after first page fault
			this->pmt->pointers[i]->descriptors[j].disk = this->kernelSystem->allocateCluster();
		}
	}

	return OK;
}

Status KernelProcess::loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void* content) {
	// Check if virtual address is valid and on page's beginning
	if (startAddress & validVaMask) return TRAP;
	if (startAddress & wordMask) return TRAP;
	VirtualAddress endAddress = startAddress + ((segmentSize - 1) << WORD);
	// Check if end virtual address is valid
	if (endAddress & validVaMask) return TRAP;
	unsigned startPage1 = (startAddress & page1Mask) >> (PAGE2 + WORD);
	unsigned endPage1 = (endAddress & page1Mask) >> (PAGE2 + WORD);
	unsigned counter = 0;
	for (unsigned i = startPage1; i <= endPage1; i++) {
		if (this->pmt->pointers[i] == 0) counter++; // count all pmt entries that have to be allocated
	}
	if (counter > 0 && this->kernelSystem->pmtAllocator->noSpace(counter)) return TRAP;
	if (this->kernelSystem->partAllocator->noSpace(segmentSize)) return TRAP;
	for (unsigned i = startPage1; i <= endPage1; i++) {
		if (this->pmt->pointers[i] == 0) {
			// Allocate all pmt2 entries
			this->pmt->pointers[i] = (PMT2 *)this->kernelSystem->allocatePMT();
			for (unsigned j = 0; j < pmt2Size; this->pmt->pointers[i]->descriptors[j++].SEGMENT = 0);
		}
		else {
			// Check all pmt2 entries if already in another segment
			unsigned startPage2 = (i == startPage1) ? ((startAddress & page2Mask) >> WORD) : 0;
			unsigned endPage2 = (i == endPage1) ? ((endAddress & page2Mask) >> WORD) : (pmt2Size - 1);
			for (unsigned j = startPage2; j <= endPage2; j++)
				if (this->pmt->pointers[i]->descriptors[j].SEGMENT == 1) return TRAP;
		}
	}
	// Set segment's descriptors and load content to cluster
	unsigned k = 0;
	for (unsigned i = startPage1; i <= endPage1; i++) {
		unsigned startPage2 = (i == startPage1) ? ((startAddress & page2Mask) >> WORD) : 0;
		unsigned endPage2 = (i == endPage1) ? ((endAddress & page2Mask) >> WORD) : (pmt2Size - 1);
		for (unsigned j = startPage2; j <= endPage2; j++) {
			this->pmt->pointers[i]->descriptors[j].V = 0;
			this->pmt->pointers[i]->descriptors[j].D = 0;
			this->pmt->pointers[i]->descriptors[j].RWX = this->accessType2RWX(flags);
			this->pmt->pointers[i]->descriptors[j].SEGMENT = 1;
			this->pmt->pointers[i]->descriptors[j].SEGMENT_START = (i == startPage1 && j == startPage2) ? 1 : 0;
			this->pmt->pointers[i]->descriptors[j].SEGMENT_SHARED = 0;
			this->pmt->pointers[i]->descriptors[j].REF = 0;
			this->pmt->pointers[i]->descriptors[j].REF_HISTORY = 0;
			this->pmt->pointers[i]->descriptors[j].block = 0; // will be changed after first page fault
			this->pmt->pointers[i]->descriptors[j].disk = this->kernelSystem->allocateCluster();
			this->kernelSystem->partition->writeCluster(this->pmt->pointers[i]->descriptors[j].disk, (char *)content + ((k++) << WORD));
		}
	}

	return OK;
}

Status KernelProcess::deleteSegment(VirtualAddress startAddress) {
	// Check if virtual address is valid and on page's beginning
	if (startAddress & validVaMask) return TRAP;
	if (startAddress & wordMask) return TRAP;
	unsigned startPage1 = (startAddress & page1Mask) >> (PAGE2 + WORD);
	unsigned startPage2 = (startAddress & page2Mask) >> WORD;
	if (this->pmt->pointers[startPage1] == nullptr) return TRAP;
	Descriptor *startDescriptor = &(this->pmt->pointers[startPage1]->descriptors[startPage2]);
	if (startDescriptor->SEGMENT == 0) return TRAP;
	if (startDescriptor->SEGMENT_START != 1) return TRAP;
	//if (startDescriptor->SEGMENT_SHARED == 1) return this->disconnectSharedSegment(this->getSharedSegmentName(startAddress));
	if (startDescriptor->SEGMENT_SHARED == 1) return TRAP;
	unsigned i = startPage1;
	while (1) {
		if (this->pmt->pointers[i] == nullptr) break;
		bool endSegment = false;
		for (unsigned j = (i == startPage1) ? startPage2 : 0; j < pmt2Size; j++) {
			bool first = (i == startPage1 && j == startPage2);
			if ((this->pmt->pointers[i]->descriptors[j].SEGMENT == 0) || (!first && this->pmt->pointers[i]->descriptors[j].SEGMENT_START == 1)) {
				endSegment = true;
				break;
			}
			this->pmt->pointers[i]->descriptors[j].SEGMENT = 0;
			if (this->pmt->pointers[i]->descriptors[j].V == 1) {
				this->kernelSystem->validDescriptorList->removeDescriptor(&this->pmt->pointers[i]->descriptors[j]);
				this->kernelSystem->deallocateBlock(this->pmt->pointers[i]->descriptors[j].block);
			}
			this->kernelSystem->deallocateCluster(this->pmt->pointers[i]->descriptors[j].disk);
		}
		// Deallocate pmt2 if all entries are free
		bool free = true;
		for(unsigned j = 0; j < pmt2Size; j++)
			if (this->pmt->pointers[i]->descriptors[j].SEGMENT == 1) {
				free = false;
				break;
			}
		if (free) {
			this->kernelSystem->deallocatePMT((PhysicalAddress)this->pmt->pointers[i]);
			this->pmt->pointers[i] = nullptr;
		}
		if (endSegment) break;
		i++;
	}

	return OK;
}

Process * KernelProcess::clone(ProcessId pid) {
	// Find process with pid
	Process *process = this->kernelSystem->processList->getProcess(pid);
	if (process == nullptr) return nullptr;
	Process *newProcess = process->pProcess->kernelSystem->createProcess();
	unsigned counter = 0;
	for (unsigned i = 0; i < pmt1Size; i++) {
		if (process->pProcess->pmt->pointers[i] != 0) counter++; // count all pmt entries that have to be allocated
	}
	if (counter > 0 && process->pProcess->kernelSystem->pmtAllocator->noSpace(counter)) return nullptr;

	for (unsigned i = 0; i < pmt1Size; i++) {
		if (process->pProcess->pmt->pointers[i] != 0) {
			// Allocate all pmt2 entries
			newProcess->pProcess->pmt->pointers[i] = (PMT2 *)newProcess->pProcess->kernelSystem->allocatePMT();
			for (unsigned j = 0; j < pmt2Size; newProcess->pProcess->pmt->pointers[i]->descriptors[j++].SEGMENT = 0);
		}
	}

	for (unsigned i = 0; i < pmt1Size; i++) {
		if (process->pProcess->pmt->pointers[i] != 0) {
			for (unsigned j = 0; j < pmt2Size; j++) {
				Descriptor *oldDescriptor = &(process->pProcess->pmt->pointers[i]->descriptors[j]);
				if (oldDescriptor->SEGMENT == 1) {
					if (oldDescriptor->SEGMENT_SHARED == 1) {
						if (oldDescriptor->SEGMENT_START == 1) {
							VirtualAddress startAddress = ((i << PAGE2) + j) << WORD;
							const char *segmentName = process->pProcess->getSharedSegmentName(startAddress);
							PageNum segmentSize = process->pProcess->kernelSystem->descriptors[segmentName]->getSize();
							AccessType flags = process->pProcess->rwx2AccessType(process->pProcess->kernelSystem->descriptors[segmentName]->getRWX());
							newProcess->pProcess->createSharedSegment(startAddress, segmentSize, segmentName, flags);
						}
						else
							continue;
					}
					else {
						Descriptor *descriptor = &(newProcess->pProcess->pmt->pointers[i]->descriptors[j]);
						descriptor->V = 0;
						descriptor->D = 0;
						descriptor->RWX = oldDescriptor->RWX;
						descriptor->SEGMENT = oldDescriptor->SEGMENT;
						descriptor->SEGMENT_START = oldDescriptor->SEGMENT_START;
						descriptor->SEGMENT_SHARED = 0;
						descriptor->REF = 0;
						descriptor->REF_HISTORY = 0;
						descriptor->block = 0;
						descriptor->disk = newProcess->pProcess->kernelSystem->allocateCluster();

						char *content;
						if (oldDescriptor->V == 1)
							content = (char *)(process->pProcess->kernelSystem->processVMSpace) + (oldDescriptor->block << WORD);
						else {
							content = new char[PAGE_SIZE];
							process->pProcess->kernelSystem->partition->readCluster(oldDescriptor->disk, content);
						}
						// Write content to new cluster
						newProcess->pProcess->kernelSystem->partition->writeCluster(descriptor->disk, content);
						if (oldDescriptor->V == 0) delete content;
					}
				}
			}
		}
	}

	return newProcess;
}

Status KernelProcess::createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char * name, AccessType flags) {
	if (this->kernelSystem->checkSharedSegment(name)) {
		// Already created
		if (this->kernelSystem->descriptors[name]->getRWX() != this->accessType2RWX(flags)) return TRAP;
	}
	else {
		// Create segment
		this->kernelSystem->createSharedSegment(name, segmentSize, this->accessType2RWX(flags));
	}

	if (startAddress & validVaMask) return TRAP;
	if (startAddress & wordMask) return TRAP;
	VirtualAddress endAddress = startAddress + ((segmentSize - 1) << WORD);
	if (endAddress & validVaMask) return TRAP;
	unsigned startPage1 = (startAddress & page1Mask) >> (PAGE2 + WORD);
	unsigned endPage1 = (endAddress & page1Mask) >> (PAGE2 + WORD);
	unsigned counter = 0;
	for (unsigned i = startPage1; i <= endPage1; i++) {
		if (this->pmt->pointers[i] == 0) counter++; // count all pmt entries that have to be allocated
	}
	if (counter > 0 && this->kernelSystem->pmtAllocator->noSpace(counter)) return TRAP;
	for (unsigned i = startPage1; i <= endPage1; i++) {
		if (this->pmt->pointers[i] == 0) {
			this->pmt->pointers[i] = (PMT2 *)this->kernelSystem->allocatePMT();
			for (unsigned j = 0; j < pmt2Size; this->pmt->pointers[i]->descriptors[j++].SEGMENT = 0);
		}
		else {
			unsigned startPage2 = (i == startPage1) ? ((startAddress & page2Mask)) >> WORD : 0;
			unsigned endPage2 = (i == endPage1) ? ((endAddress & page2Mask) >> WORD) : (pmt2Size - 1);
			for (unsigned j = startPage2; j <= endPage2; j++)
				if (this->pmt->pointers[i]->descriptors[j].SEGMENT == 1) return TRAP;
		}
	}
	// Set segment's descriptors
	unsigned offset = 0;
	for (unsigned i = startPage1; i <= endPage1; i++) {
		unsigned startPage2 = (i == startPage1) ? ((startAddress & page2Mask) >> WORD) : 0;
		unsigned endPage2 = (i == endPage1) ? ((endAddress & page2Mask) >> WORD) : (pmt2Size - 1);
		for (unsigned j = startPage2; j <= endPage2; j++) {
			this->kernelSystem->descriptors[name]->setSharedDescriptor(&(this->pmt->pointers[i]->descriptors[j]), offset << WORD);
			offset++;
		}
	}

	// Connect process to shared segment's descriptor
	this->kernelSystem->descriptors[name]->connectProcess(this->process, startAddress);

	return OK;
}

Status KernelProcess::disconnectSharedSegment(const char * name) {
	if (this->kernelSystem->checkSharedSegment(name) == false) 
		return TRAP;
	VirtualAddress startAddress = this->kernelSystem->descriptors[name]->getStartAddress(this->process);
	unsigned startPage1 = (startAddress & page1Mask) >> (PAGE2 + WORD);
	unsigned startPage2 = (startAddress & page2Mask) >> WORD;
	unsigned i = startPage1;
	while (1) {
		if (this->pmt->pointers[i] == nullptr) break;
		bool endSegment = false;
		for (unsigned j = (i == startPage1) ? startPage2 : 0; j < pmt2Size; j++) {
			bool first = (i == startPage1 && j == startPage2);
			if ((this->pmt->pointers[i]->descriptors[j].SEGMENT == 0) || (!first && this->pmt->pointers[i]->descriptors[j].SEGMENT_START == 1)) {
				endSegment = true;
				break;
			}
			this->pmt->pointers[i]->descriptors[j].SEGMENT = 0;
			if (this->kernelSystem->descriptors[name]->isLast(this->process)) {
				if (this->pmt->pointers[i]->descriptors[j].V == 1) {
					if (this->pmt->pointers[i]->descriptors[j].D == 1)
						this->kernelSystem->partition->writeCluster(this->pmt->pointers[i]->descriptors[j].disk, (char*)(this->kernelSystem->processVMSpace) + (this->pmt->pointers[i]->descriptors[j].block << WORD));
					this->kernelSystem->deallocateBlock(this->pmt->pointers[i]->descriptors[j].block);
				}
			}
			if (this->pmt->pointers[i]->descriptors[j].V == 1)
				this->kernelSystem->validDescriptorList->removeDescriptor(&this->pmt->pointers[i]->descriptors[j]);
		}
		// Deallocate pmt2 if all entries are free
		bool free = true;
		for (unsigned j = 0; j < pmt2Size; j++)
			if (this->pmt->pointers[i]->descriptors[j].SEGMENT == 1) {
				free = false;
				break;
			}
		if (free) {
			this->kernelSystem->deallocatePMT((PhysicalAddress)this->pmt->pointers[i]);
			this->pmt->pointers[i] = nullptr;
		}
		if (endSegment) break;
		i++;
	}

	this->kernelSystem->descriptors[name]->disconnectProcess(this->process);

	return OK;
}

Status KernelProcess::deleteSharedSegment(const char * name) {
	if (this->kernelSystem->checkSharedSegment(name) == false) return TRAP;
	this->kernelSystem->descriptors[name]->disconnectAllProcesses();
	delete this->kernelSystem->descriptors[name];
	this->kernelSystem->descriptors[name] = nullptr;

	return OK;
}

Status KernelProcess::pageFault(VirtualAddress address) {
	// Check if virtual address is valid
	if (address & validVaMask) return TRAP;
	unsigned page1 = (address & page1Mask) >> (PAGE2 + WORD);
	unsigned page2 = (address & page2Mask) >> WORD;
	if (this->pmt->pointers[page1] == nullptr) return TRAP;
	Descriptor *descriptor = &(this->pmt->pointers[page1]->descriptors[page2]);
	if (descriptor->SEGMENT == 0) return TRAP;
	// Check page fault
	if (descriptor->V == 1) return OK;
	// Allocate block for page
	PageNum block = this->kernelSystem->allocateBlock();

	if (descriptor->SEGMENT_SHARED == 1) {
		// Update same descriptors from other processes (and kernel system)
		ClusterNo cluster = descriptor->disk;
		SharedSegmentDescriptor *d = this->kernelSystem->descriptors[this->getSharedSegmentName(address)];
		d->updateAllDescriptors(d->getOffset(this->process, address), block);
	}
	else {
		unsigned refBitMask = 0x80000000;
		// Set page's descriptor
		descriptor->V = 1;
		descriptor->D = 0;
		descriptor->REF = 0;
		descriptor->REF_HISTORY = refBitMask;
		descriptor->block = block;
		// Update kernel system
		this->kernelSystem->validDescriptorList->putDescriptor(descriptor);
	}

	// Load block from swap partition
	this->kernelSystem->partition->readCluster(descriptor->disk, ((char *)this->kernelSystem->processVMSpace + (descriptor->block << WORD)));
	
	return OK;
}

PhysicalAddress KernelProcess::getPhysicalAddress(VirtualAddress address) {
	// Check if virtual address is valid
	if(address & validVaMask) return 0;
	unsigned page1 = (address & page1Mask) >> (PAGE2 + WORD);
	unsigned page2 = (address & page2Mask) >> WORD;
	unsigned word = address & wordMask;
	if (this->pmt->pointers[page1] == nullptr) return 0;
	Descriptor *descriptor = &(this->pmt->pointers[page1]->descriptors[page2]);
	if (descriptor->SEGMENT == 0) return 0;
	// Check page fault
	if (descriptor->V == 0) return 0;
	return (PhysicalAddress)((char *)this->kernelSystem->processVMSpace + (descriptor->block << WORD) + word);
}

Status KernelProcess::access(VirtualAddress address, AccessType type) {
	// Check if virtual address is valid
	if(address & validVaMask) return TRAP;
	unsigned page1 = (address & page1Mask) >> (PAGE2 + WORD);
	unsigned page2 = (address & page2Mask) >> WORD;
	if (this->pmt->pointers[page1] == nullptr) return TRAP;
	Descriptor *descriptor = &(this->pmt->pointers[page1]->descriptors[page2]);
	if (descriptor->SEGMENT == 0) return TRAP;
	// Check page fault
	if (descriptor->V == 0) return PAGE_FAULT;
	// Check access
	switch (type) {
	case READ: {
		if (descriptor->RWX & RD) {
			descriptor->REF = 1;
			break;
		}
		else
			return TRAP;
	}
	case WRITE: {
		if (descriptor->RWX & WR) {
			descriptor->REF = 1;
			descriptor->D = 1;
			break;
		}
		else 
			return TRAP;
	}
	case READ_WRITE: {
		if (descriptor->RWX & RD_WR) {
			descriptor->REF = 1;
			descriptor->D = 1;
			break;
		}
		else
			return TRAP;
	}
	case EXECUTE: {
		if (descriptor->RWX & EX) {
			descriptor->REF = 1;
			break;
		}
		else
			return TRAP;
	}
	default: return TRAP;
	}

	return OK;
}

unsigned KernelProcess::accessType2RWX(AccessType flags) {
	switch (flags) {
	case READ: return RD;
	case WRITE: return WR;
	case READ_WRITE: return RD_WR;
	case EXECUTE: return EX;
	default: return RD_WR;
	}
}

AccessType KernelProcess::rwx2AccessType(unsigned RWX) {
	switch (RWX) {
	case RD: return READ;
	case WR: return WRITE;
	case RD_WR: return READ_WRITE;
	case EX: return EXECUTE;
	default: return READ_WRITE;
	}
}

const char * KernelProcess::getSharedSegmentName(VirtualAddress address) {
	for (std::pair<const char *, SharedSegmentDescriptor *> cur : this->kernelSystem->descriptors) {
		if (cur.second->isConnected(this->process, address)) {
			return cur.first;
		}
	}

	return "";
}
