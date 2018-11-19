#include "KernelSystem.h"
#include "Process.h"
#include "part.h"
#include "Allocator.h"
#include "ProcessList.h"
#include "DescriptorList.h"
#include "SharedSegmentDescriptor.h"

ProcessId KernelSystem::pidGenerator = 0;

KernelSystem::KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize, PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition* partition, System *system)
: processVMSpace(processVMSpace), pmtSpace(pmtSpace), partition(partition), system(system) {
	this->vmAllocator = new Allocator(processVMSpaceSize);
	this->pmtAllocator = new Allocator(pmtSpaceSize);
	this->partAllocator = new Allocator(partition->getNumOfClusters());
	this->processList = new ProcessList();
	this->validDescriptorList = new DescriptorList();
}

KernelSystem::~KernelSystem() {
	delete this->processList;
	delete this->validDescriptorList;
	delete this->vmAllocator;
	delete this->pmtAllocator;
	delete this->partAllocator;

	this->processList = nullptr;
	this->vmAllocator = nullptr;
	this->pmtAllocator = nullptr;
	this->partAllocator = nullptr;
	this->validDescriptorList = nullptr;
}

Process* KernelSystem::createProcess() {
	if (pmtAllocator->noSpace()) return 0;
	Process *process = new Process(this->pidGenerator++);
	this->processList->putProcess(process);
	process->pProcess->setKernelSystem(this);
	process->pProcess->setPMT1();

	return process;
}

Process* KernelSystem::cloneProcess(ProcessId pid) {
	Process *process = this->processList->getProcess(pid);
	if (process == nullptr) return nullptr;
	return process->clone(process->getProcessId());
}

Time KernelSystem::periodicJob() {
	unsigned refBitMask = 0x80000000;
	this->validDescriptorList->resetCursor();
	while (!this->validDescriptorList->end()) {
		Descriptor *cur = this->validDescriptorList->getNextDescriptor();
		cur->REF_HISTORY >>= 1;
		if (cur->REF == 1) {
			cur->REF_HISTORY |= refBitMask;
			cur->REF = 0;
		}
	}

	return nextTime;
}

Status KernelSystem::access(ProcessId pid, VirtualAddress address, AccessType type) {
	Process *process = this->processList->getProcess(pid);
	if (!process) return TRAP;
	return process->pProcess->access(address, type);
}

PageNum KernelSystem::allocateBlock() {
	if (this->vmAllocator->noSpace()) {
		PageNum block = this->getVictim();
		if (block >= 32) {
			block = block;
		}
		return block;
	}
	if (this->vmAllocator->size == 0 && this->vmAllocator->firstBlock->block >= 32) {
		int i = 0;
	}
	PageNum block = this->vmAllocator->allocate();
	if (block >= 32 || block == -1) {
		block = block;
		bool yey = this->vmAllocator->noSpace();
		yey = yey;
	}
	return block;
}

void KernelSystem::deallocateBlock(PageNum block) {
	this->vmAllocator->deallocate(block);
}

PhysicalAddress KernelSystem::allocatePMT() {
	if (this->pmtAllocator->noSpace()) 
		return 0;
	return (PhysicalAddress)((char *)this->pmtSpace + (this->pmtAllocator->allocate() << WORD));
}

void KernelSystem::deallocatePMT(PhysicalAddress pmt) {
	this->pmtAllocator->deallocate(((char *)pmt - (char *)this->pmtSpace) >> WORD);
}

ClusterNo KernelSystem::allocateCluster() {
	if (this->partAllocator->noSpace()) 
		return 0;
	return this->partAllocator->allocate();
}

void KernelSystem::deallocateCluster(ClusterNo cluster) {
	this->partAllocator->deallocate(cluster);
}

void KernelSystem::deleteProcess(ProcessId pid) {
	this->processList->removeProcess(pid);
}

PageNum KernelSystem::getVictim() {
	Descriptor *victim = nullptr;
	unsigned minRefHistory = 0xFFFFFFFF;

	this->validDescriptorList->resetCursor();
	while (!this->validDescriptorList->end()) {
		Descriptor *cur = this->validDescriptorList->getNextDescriptor();
		if (minRefHistory > cur->REF_HISTORY) {
			victim = cur;
			minRefHistory = cur->REF_HISTORY;
		}
	}

	if (victim == nullptr) {
		return -1;
	}

	// Save block to disk if dirty
	if (victim->D == 1) {
		this->partition->writeCluster(victim->disk, (char *)this->processVMSpace + (victim->block << WORD));
	}

	if (victim->SEGMENT_SHARED == 1) {
		// Delete same descriptors from other processes
		ClusterNo cluster = victim->disk;
		PageNum block = victim->block;
		this->validDescriptorList->resetCursor();
		Descriptor *cur = this->validDescriptorList->getNextDescriptor();
		while (cur != nullptr) {
			if (cur->disk == cluster) {
				cur->V = 0;
				cur->D = 0;
				cur->REF = 0;
				cur->REF_HISTORY = 0; // will be changed on first page fault
				this->validDescriptorList->removeDescriptor(cur);
			}
			cur = this->validDescriptorList->getNextDescriptor();
		}

		return block;
	}

	victim->V = 0;
	victim->D = 0;
	victim->REF = 0;
	victim->REF_HISTORY = 0; // will be changed on first page fault
	PageNum block = victim->block;
	this->validDescriptorList->removeDescriptor(victim);

	return block;
}

void KernelSystem::createSharedSegment(const char* name, PageNum segmentSize, unsigned RWX) {
	SharedSegmentDescriptor *descriptor = new SharedSegmentDescriptor(this, segmentSize, RWX);
	descriptors[name] = descriptor;
}
