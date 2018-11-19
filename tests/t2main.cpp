#include "testdefs.h"

#ifdef _VM_TEST_2

#include "part.h"
#include "vm_declarations.h"
#include "System.h"
#include "Process.h"
#include "assert.h"
//#include "OMAllocator.h"
//#include "DiscAllocator.h"
#include "kernelsys.h"

#include <iostream>
#include <thread>
#include <mutex>
#include <random>
#include <chrono>

using namespace std;

///MULTITHREADED correctness test

//fixed
const PageNum PMT_PAGES = 2000;
const ClusterNo NUM_CLUSTERS = 30000;
#define PAGE_BITS 10
#define ss(page) (page << PAGE_BITS)
#define PF_TIME
#define MEASURE

//changeable
const PageNum MEM_PAGES = 32;
const PageNum SEG_SIZE = 32;
const ProcessId NUM_PROC = 10;
const unsigned REP = 5;

mutex GLOBAL_LOCK;

typedef std::chrono::high_resolution_clock myclock;
myclock::time_point beginning[2 * NUM_PROC + 1];
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
	*c = 'a' + diff;
}

unsigned page_fault_counter = 0;
int numOfActiveProc = NUM_PROC; // Broj procesa.
System* sys;

void rotate(System* sys, Process* p, ProcessId pid, VirtualAddress vaddr, unsigned rep) {
	GLOBAL_LOCK.lock();
	Status status = sys->access(pid, vaddr, AccessType::READ_WRITE);
	assert(status != Status::TRAP);
	if (status == Status::PAGE_FAULT) {
		p->pageFault(vaddr);
		sys->access(pid, vaddr, AccessType::READ_WRITE);
		page_fault_counter++;
	}
	char* paddr = (char*)p->getPhysicalAddress(vaddr);
	/*for (int i = 0; i < 1024; i++) {
	cout << paddr[i];
	}
	cout << endl << endl;*/
	rotate(paddr, 1);
#ifdef MEASURE
	if (vaddr == PAGE_SIZE * SEG_SIZE - 1 && rep == 0)
		cout << pid << " : " << get_seconds_passed(pid) << endl;
#endif
	GLOBAL_LOCK.unlock();
}

int br = 0;
unsigned counterShared = 0;

void f(System* sys) {
	Process* p = sys->createProcess();
	ProcessId pid = p->getProcessId();
	GLOBAL_LOCK.lock();
	++br;
	VirtualAddress startShared = ss(SEG_SIZE + br + 1);
	assert(p->createSharedSegment(startShared, SEG_SIZE, "radi", READ_WRITE) != Status::TRAP);
	Status status = sys->access(pid, startShared, AccessType::READ_WRITE);
	assert(status != Status::TRAP);
	if (status == Status::PAGE_FAULT) {
		p->pageFault(startShared);
		sys->access(pid, startShared, AccessType::READ_WRITE);
		page_fault_counter++;
	}
	unsigned* paddr = (unsigned*)p->getPhysicalAddress(startShared);
	if (br == 1) {
		*paddr = 0;
	}
	GLOBAL_LOCK.unlock();
	//load address space [0-PG_SZ*SG_SZ)
	void* starting_content = ::operator new(PAGE_SIZE * SEG_SIZE);
	for (VirtualAddress vaddr = 0; vaddr < PAGE_SIZE * SEG_SIZE; ++vaddr)
		*((char*)starting_content + vaddr) = rand_letter();
	GLOBAL_LOCK.lock();
	assert(p->loadSegment(ss(0), SEG_SIZE, READ_WRITE, starting_content) != Status::TRAP);
	GLOBAL_LOCK.unlock();

	unsigned rep = REP;

#ifdef MEASURE
	start_timer(pid);
#endif

	///Rep times rotate address space content
	while (rep--) {

		//rotate standard seg by 1
		for (VirtualAddress vaddr = 0UL; vaddr < PAGE_SIZE * SEG_SIZE; ++vaddr)
			rotate(sys, p, pid, vaddr, rep);

		//end rep
	}
	int k = 0;

	///CHECK NOW!
	//read em in the end, check REP times rotated = content now
	for (VirtualAddress vaddr = 0UL; vaddr < PAGE_SIZE * SEG_SIZE; ++vaddr) {
		GLOBAL_LOCK.lock();

		Status status = sys->access(pid, vaddr, AccessType::READ);
		assert(status != Status::TRAP);
		if (status == Status::PAGE_FAULT)
			p->pageFault(vaddr);
		char* paddr = (char*)p->getPhysicalAddress(vaddr);
		///
		char* now = paddr;
		char* before = (char*)starting_content + vaddr;
		rotate(before, REP);

		assert(*now == *before);
		GLOBAL_LOCK.unlock();
		for (int i = 0; i<500; i++);
		GLOBAL_LOCK.lock();
		int kol = vaddr / PAGE_SIZE;

		if (kol == k) {

			k++;
			Status status = sys->access(pid, startShared, AccessType::READ_WRITE);
			assert(status != Status::TRAP);
			if (status == Status::PAGE_FAULT) {
				p->pageFault(startShared);
				sys->access(pid, startShared, AccessType::READ_WRITE);
				page_fault_counter++;
			}
			unsigned* paddr = (unsigned*)p->getPhysicalAddress(startShared);
			unsigned counter = *paddr;
			counter++;
			if (counter > counterShared) {
				counterShared = counter;
				cout << counter << endl;
			}

			*paddr = counter;
		}
		GLOBAL_LOCK.unlock();
	}
	numOfActiveProc--;
	delete p;
}

void timeTest() {
	while (numOfActiveProc) {
		GLOBAL_LOCK.lock();
		Time time = sys->periodicJob();
		GLOBAL_LOCK.unlock();
		std::this_thread::sleep_for(std::chrono::microseconds(time));
	}
}

int main() {

	char dummy[1024];
	void * p2 = malloc(PAGE_SIZE * MEM_PAGES);
	void * px = malloc(100000);
	void* pmtSpace = malloc(PAGE_SIZE * PMT_PAGES);
	std::cout << "adresa: " << p2 << "\n";
	size_t size = 10240;
	void * memSpace = std::align(1024, sizeof(dummy), p2, size);


	Partition* part = new Partition("p1.ini");

	sys = new System(memSpace, MEM_PAGES, pmtSpace, PMT_PAGES, part);
	//Partition* part = new Partition("p1.ini");
	//void* pmtSpace = ::operator new(PAGE_SIZE * PMT_PAGES);
	//void* memSpace = ::operator new(PAGE_SIZE * MEM_PAGES);
	//sys = new System(memSpace, MEM_PAGES, pmtSpace, PMT_PAGES, part);
	start_timer(20);
	thread periodicJobTimer(timeTest);

	thread** thr;
	thr = new thread*[NUM_PROC];
	for (ProcessId pid = 0; pid < NUM_PROC; ++pid)
		thr[pid] = new thread(f, sys);

	for (ProcessId pid = 0; pid < NUM_PROC; ++pid)
		thr[pid]->join();


	cout << "TOTAL PAGE FAULTS: " << page_fault_counter << endl;
	cout << get_seconds_passed(20);
	periodicJobTimer.join();
	delete sys;
	//cout << "Broj PMT blokova = "; KernelSystem::allocPMTSpace->pisi(); cout << endl;
	//cout << "Broj PRO blokova = "; KernelSystem::allocProcSpace->pisi(); cout << endl;
	//cout << "Broj DSC blokova = "; KernelSystem::dscAlloc->pisi(); cout << endl;
	int a;
	cin >> a;
	return 0;
}

#endif