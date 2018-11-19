#include "testdefs.h"

#ifdef _VM_TEST_1

#include "part.h"
#include "vm_declarations.h"
#include "System.h"
#include "Process.h"
#include "assert.h"

#include <iostream>
#include <thread>
#include <mutex>
#include <random>
#include <chrono>

using namespace std;

///MULTITHREADED correctness test
#define SHARED
//#undef SHARED

#define CLONE
//#undef CLONE

#ifdef SHARED
const PageNum SHARED_SEG_SIZE = 32;
void* shared_seg_content;
#endif

//fixed
const PageNum PMT_PAGES = 2000;
const ClusterNo NUM_CLUSTERS = 10000;
#define PAGE_BITS 10
#define ss(page) (page << PAGE_BITS)
#define PF_TIME
#define MEASURE

//changeable
const PageNum MEM_PAGES = 32;
const PageNum SEG_SIZE = 32;
const ProcessId NUM_PROC = 100;
const unsigned REP = 5;

mutex GLOBAL_LOCK;

typedef std::chrono::high_resolution_clock myclock;
myclock::time_point beginning[10 * NUM_PROC + 1];
default_random_engine generator;
uniform_int_distribution<int> distribution(0, 25);

void start_timer(ProcessId pid) {
	beginning[pid] = myclock::now();
}

double get_seconds_passed(ProcessId pid) {
	myclock::duration d = myclock::now() - beginning[pid];
	return  d.count() * 1.0 / 1000000000;
}

char rand_letter() {
	return 'a' + distribution(generator);
}

void rotate(char* c, int n) { //rotate this character n times
	int diff = (*c) - 'a';
	diff = (diff + n) % 26;
	if (diff < 0)
		diff += 26;
	*c = 'a' + diff;
}

unsigned page_fault_counter = 0;

void rotate(System* sys, Process* p, ProcessId pid, VirtualAddress vaddr, unsigned rep) {
	GLOBAL_LOCK.lock();
	Status status = sys->access(pid, vaddr, AccessType::READ_WRITE);
	assert(status != Status::TRAP);
	if (status == Status::PAGE_FAULT) {
		p->pageFault(vaddr);
		page_fault_counter++;
	}
	char* paddr = (char*)p->getPhysicalAddress(vaddr);
	rotate(paddr, 1);
#ifdef MEASURE
	if (vaddr == PAGE_SIZE * SEG_SIZE - 1 && rep == 0)
		cout << pid << " : " << get_seconds_passed(pid) << endl;
#endif
	GLOBAL_LOCK.unlock();
}

void f(System* sys) {
	Process* p = sys->createProcess();
	ProcessId pid = p->getProcessId();

	//load address space [0-PG_SZ*SG_SZ)
	void* starting_content = ::operator new(PAGE_SIZE * SEG_SIZE);
	for (VirtualAddress vaddr = 0; vaddr < PAGE_SIZE * SEG_SIZE; ++vaddr)
		*((char*)starting_content + vaddr) = rand_letter();
	
	GLOBAL_LOCK.lock();
	assert(p->loadSegment(ss(0), SEG_SIZE, READ_WRITE, starting_content) != Status::TRAP);
	GLOBAL_LOCK.unlock();
#ifdef SHARED
	//connect shared segment
	p->createSharedSegment(ss((SEG_SIZE + pid)), SHARED_SEG_SIZE, "ss", READ_WRITE);
#endif

#ifdef CLONE
	Process* klon = sys->cloneProcess(pid);
	assert(klon != nullptr);
	ProcessId klonId = klon->getProcessId();
#endif

	unsigned rep = REP;

#ifdef MEASURE
	start_timer(pid);
#ifdef CLONE
	start_timer(klonId);
#endif
#endif

	///Rep times rotate address space content
	while (rep--) {
#ifdef SHARED
		//rotate shared seg by 1 -> odd processes
		if (pid & 1) {
			for (VirtualAddress vaddr = ss((SEG_SIZE + pid)); vaddr < ss((SEG_SIZE + pid)) + SHARED_SEG_SIZE * PAGE_SIZE; ++vaddr) {
				rotate(sys, p, pid, vaddr, rep);
#ifdef CLONE
				rotate(sys, klon, klonId, vaddr, rep);
#endif
			}
		}
#endif

		//rotate standard seg by 1
		for (VirtualAddress vaddr = 0UL; vaddr < PAGE_SIZE * SEG_SIZE; ++vaddr) {
			rotate(sys, p, pid, vaddr, rep);
#ifdef CLONE
			rotate(sys, klon, klonId, vaddr, rep);
#endif
		}

#ifdef SHARED
		//rotate shared seg by 1 -> even processes
		if (!(pid & 1)) {
			for (VirtualAddress vaddr = ss((SEG_SIZE + pid)); vaddr < ss((SEG_SIZE + pid)) + SHARED_SEG_SIZE * PAGE_SIZE; ++vaddr) {
				rotate(sys, p, pid, vaddr, rep);
#ifdef CLONE
				rotate(sys, klon, klonId, vaddr, rep);
#endif
			}
		}
#endif
		//end rep
	}

	///CHECK NOW!
	//read em in the end, check REP times rotated = content now
	for (VirtualAddress vaddr = 0UL; vaddr < PAGE_SIZE * SEG_SIZE; ++vaddr) {
		GLOBAL_LOCK.lock();
		Status status = sys->access(pid, vaddr, READ);
		assert(status != Status::TRAP);
		if (status == Status::PAGE_FAULT)
			p->pageFault(vaddr);
		char* paddr = (char*)p->getPhysicalAddress(vaddr);
		///
		char* now = paddr;
		char* before = (char*)starting_content + vaddr;
		rotate(before, REP);
		assert(*now == *before);
		rotate(before, -(int)REP);
		GLOBAL_LOCK.unlock();
	}

#ifdef CLONE
	//check for cloned process -> create recursive clone and just read
	Process* kk = sys->cloneProcess(klonId);
	ProcessId kkid = kk->getProcessId();

	Process* kkk = sys->cloneProcess(kkid);
	ProcessId kkkid = kkk->getProcessId();

	for (VirtualAddress vaddr = 0UL; vaddr < PAGE_SIZE * SEG_SIZE; ++vaddr) {
		GLOBAL_LOCK.lock();
		Status status = sys->access(kkkid, vaddr, READ);
		assert(status != Status::TRAP);
		if (status == Status::PAGE_FAULT)
			kkk->pageFault(vaddr);
		char* paddr = (char*)kkk->getPhysicalAddress(vaddr);
		///
		char* now = paddr;
		char* before = (char*)starting_content + vaddr;
		rotate(before, REP);
		assert(*now == *before);
		rotate(before, -(int)REP);
		GLOBAL_LOCK.unlock();
	}
#endif
	delete p;
}

int main() {
	Partition* part = new Partition("p1.ini");
	void* pmtSpace = ::operator new(PAGE_SIZE * PMT_PAGES);
	void* memSpace = ::operator new(PAGE_SIZE * MEM_PAGES);
	System* sys = new System(memSpace, MEM_PAGES, pmtSpace, PMT_PAGES, part);
	start_timer(20);
#ifdef SHARED
	Process* dummy = sys->createProcess();
	dummy->createSharedSegment(ss(SEG_SIZE), SHARED_SEG_SIZE, "ss", READ_WRITE);
	//write initial values by dummy
	shared_seg_content = ::operator new(PAGE_SIZE * SHARED_SEG_SIZE);

	//write down initial random content to shared_seg_vaddr_space
	for (VirtualAddress vaddr = ss(SEG_SIZE); vaddr < ss(SEG_SIZE) + SHARED_SEG_SIZE * PAGE_SIZE; ++vaddr) {
		*((char*)shared_seg_content + vaddr - ss(SEG_SIZE)) = rand_letter();
		Status status = sys->access(dummy->getProcessId(), vaddr, WRITE);
		assert(status != Status::TRAP);
		if (status == Status::PAGE_FAULT)
			dummy->pageFault(vaddr);
		char* paddr = (char*)dummy->getPhysicalAddress(vaddr);
		*paddr = ((char*)shared_seg_content)[vaddr - ss(SEG_SIZE)];
	}

	//check if ya read it good
	Process* dummy2 = sys->createProcess();
	dummy2->createSharedSegment(ss(SEG_SIZE), SHARED_SEG_SIZE, "ss", READ); //connect it for reading
	for (VirtualAddress vaddr = ss(SEG_SIZE); vaddr < ss(SEG_SIZE) + SHARED_SEG_SIZE * PAGE_SIZE; ++vaddr) {
		Status status = sys->access(dummy2->getProcessId(), vaddr, AccessType::READ);
		assert(status != Status::TRAP);
		if (status == Status::PAGE_FAULT)
			dummy2->pageFault(vaddr);
		char* paddr = (char*)dummy2->getPhysicalAddress(vaddr);
		char* paddrtest = (char*)dummy->getPhysicalAddress(vaddr);
		assert(paddr == paddrtest);
		assert(*paddr == ((char*)shared_seg_content)[vaddr - ss(SEG_SIZE)]);
	}
	dummy2->disconnectSharedSegment("ss");
#endif

	thread** thr;
	thr = new thread*[NUM_PROC];
	for (ProcessId pid = 0; pid < NUM_PROC; ++pid)
		thr[pid] = new thread(f, sys);

	for (ProcessId pid = 0; pid < NUM_PROC; ++pid)
		thr[pid]->join();

#ifdef SHARED
	///CHECK shared content
	for (VirtualAddress vaddr = ss(SEG_SIZE); vaddr < ss(SEG_SIZE) + SHARED_SEG_SIZE * PAGE_SIZE; ++vaddr) {
		Status status = sys->access(dummy->getProcessId(), vaddr, READ);
		assert(status != Status::TRAP);
		if (status == Status::PAGE_FAULT)
			dummy->pageFault(vaddr);
		char* paddr = (char*)dummy->getPhysicalAddress(vaddr);
		///
		char* now = paddr;
		char* before = (char*)shared_seg_content + vaddr - ss(SEG_SIZE);
		unsigned ROTATE_COUNT = REP*NUM_PROC;
#ifdef CLONE
		ROTATE_COUNT *= 2;
#endif
		rotate(before, ROTATE_COUNT);
		assert(*now == *before);
	}
#endif

	cout << "TOTAL PAGE FAULTS: " << page_fault_counter << endl;
	cout << get_seconds_passed(20);
	delete sys;
	return 0;
}

#endif