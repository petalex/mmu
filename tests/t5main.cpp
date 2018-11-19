#include "testdefs.h"

#include<iostream>
#include<cassert>
//#include "page_tables.h"
#include<mutex>
#include<list>
#include<cstdlib>
#include<ctime>
#include<thread>
#include<chrono>
#include "vm_declarations.h"
#include "KernelSys.h"
//#include "Swap.h"
#include "Process.h"
#include "System.h"
using namespace std;

#ifdef _VM_TEST_5

std::mutex mtx;

#define RQD(r,t) if((r)==(t)){std::cout<<(r)<<' '<<(t)<<' '<<__LINE__<<'\n';exit(1);}
#define RQE(r,t) if((r)!=(t)){std::cout<<(r)<<' '<<(t)<<' '<<__LINE__<<'\n'; throw std::exception(); exit(1);}


#define MEMORY_CAPACITY_P 1000
#define MEMORY_CAPACITY (MEMORY_CAPACITY_P*1024)
#define PROC_COUNT 10
#define ITER1 1000
#define ITER2 1000
#define LOOPS 2

volatile bool running = true;
int p = 0;
void periodical(System *sys)
{
	while (running)
	{
		std::this_thread::sleep_for(std::chrono::microseconds(sys->periodicJob()));
		p++;
	}
}
unsigned long long fc = 0;
unsigned long ac = 0;
int z = 0;
const char *ime;
const char *ime1 = "Seg1";
const char *ime2 = "Seg2";
void processBody(System *sys, Process *proc, char *initial, int workset)
{
	PageNum start = rand() % 10;
	mtx.lock();
	std::cout << "Chose start as " << start << std::endl;
	auto status = proc->createSharedSegment(start * 1024, MEMORY_CAPACITY_P, ime, READ_WRITE);
	std::cout << "Connected to shared segment." << std::endl;
	assert(status == OK);
	if (!z)
	{
		std::cout << "Filling segment with shit." << std::endl;
		for (int i = 0; i < MEMORY_CAPACITY; i++)
		{
			if (i % 1000 == 0)
				std::cout << "At address " << i << std::endl;
			VirtualAddress addr = start * 1024 + i;
			ac++;
			auto result = sys->access(proc->getProcessId(), addr, WRITE);
			RQD(result, TRAP);
			if (result == PAGE_FAULT)
			{
				fc++;
				result = proc->pageFault(addr);
				RQE(result, OK);
			}
			result = sys->access(proc->getProcessId(), addr, WRITE);
			if (result != OK) std::cout << i << '\n';
			RQE(result, OK);
			void *stored = proc->getPhysicalAddress(addr);
			RQD(stored, (void*)nullptr);
			char *storedc = (char*)stored;
			char gen = rand() % 128;
			initial[addr - start * 1024] = gen;
			*storedc = gen;
		}
		z = 1;
		std::cout << "Done creating shared segment starting content." << std::endl;
	}
	std::cout << "Creating segment." << std::endl;
	status = proc->createSegment(start * 1024 + MEMORY_CAPACITY_P * 1024 + 1024, 5, READ_WRITE);
	if (status != OK)
	{

	}
	assert(status == OK);
	std::cout << "Done creating segment." << std::endl;
	mtx.unlock();
	int *w = new int[workset];
	for (int i = 0; i < ITER1; i++)
	{
		for (int j = 0; j < workset; j++) w[j] = rand() % MEMORY_CAPACITY_P;
		for (int j = 0; j<ITER2; j++)
		{
			{
				mtx.lock();
				ac++;
				VirtualAddress addr = start * 1024 + w[rand() % workset] * 1024 + rand() % 1024;
				auto result = sys->access(proc->getProcessId(), addr, READ);
				RQD(result, TRAP);
				if (result == PAGE_FAULT)
				{
					fc++;
					result = proc->pageFault(addr);
					RQE(result, OK);
				}
				result = sys->access(proc->getProcessId(), addr, READ);
				if (result != OK) std::cout << i << '\n';
				RQE(result, OK);
				void *stored = proc->getPhysicalAddress(addr);
				RQD(stored, (void*)nullptr);
				char *storedc = (char*)stored;
				if (addr - start * 1024 > MEMORY_CAPACITY) {
					cout << "FUUUUUUUUUUUCK!";
					assert(false);
				}
				RQE(*storedc, initial[addr - start * 1024]);
				mtx.unlock();
			}
			{
				mtx.lock();
				ac++;
				VirtualAddress addr = start * 1024 + w[rand() % workset] * 1024 + rand() % 1024;
				auto result = sys->access(proc->getProcessId(), addr, WRITE);
				RQD(result, TRAP);
				if (result == PAGE_FAULT)
				{
					fc++;
					result = proc->pageFault(addr);
					RQE(result, OK);
				}
				result = sys->access(proc->getProcessId(), addr, WRITE);
				if (result != OK) std::cout << i << '\n';
				RQE(result, OK);
				void *stored = proc->getPhysicalAddress(addr);
				RQD(stored, (void*)nullptr);
				char *storedc = (char*)stored;
				char gen = rand() % 128;
				initial[addr - start * 1024] = gen;
				*storedc = gen;
				mtx.unlock();
			}
		}
	}
	mtx.lock();
	delete proc;
	std::cout << "\nPROCDONE " << workset << "\n";
	mtx.unlock();
	delete[] w;
}

int main()
{
	//cout << sizeof(PageDescriptor);
	//return 0;
	char **initial = new char*[PROC_COUNT];
	void *phymem = new char[(MEMORY_CAPACITY * 20) + PAGE_SIZE * 2];
	char *pmtmem = new char[102400000];
	initial[0] = new char[MEMORY_CAPACITY];
	for (int i = 0; i < MEMORY_CAPACITY; i++)
	{
		initial[0][i] = rand() % 128;
	}
	for (int j = 1; j < PROC_COUNT; j++)
	{
		initial[j] = new char[MEMORY_CAPACITY];
		memcpy(initial[j], initial[0], MEMORY_CAPACITY);
	}
	auto part = new Partition("p1.ini");

	char dummy[1024];


	//size_t size = MEMORY_CAPACITY * 20 + PAGE_SIZE * 2;
	void * memSpace = phymem;


	//Partition* part = new Partition("p1.ini");

	//sys = new System(memSpace, MEM_PAGES, pmtSpace, PMT_PAGES, part);

	System *sys = new System(memSpace, MEMORY_CAPACITY_P / 2, pmtmem, 100000, part);
	auto pThrad = std::thread(periodical, sys);
	std::list<Process*> processes;
	ime = ime1;
	for (int z = 0; z < LOOPS; z++)
	{
		auto proc0 = sys->createProcess();
		::z = 0;
		for (int j = 1; j < PROC_COUNT; j++)
		{
			initial[j] = new char[MEMORY_CAPACITY];
			memcpy(initial[j], initial[0], MEMORY_CAPACITY);
		}
		processes.push_back(proc0);
		std::list<std::thread*> threads;
		for (int i = 1; i < PROC_COUNT; i++)
		{
			mtx.lock();
			//auto proc = proc0->clone(proc0->getProcessId());
			auto proc = sys->cloneProcess(proc0->getProcessId());
			mtx.unlock();
			assert(proc != nullptr);
			processes.push_back(proc);
			proc0 = proc;
		}

		auto iter = processes.begin();
		for (int i = 0; i < PROC_COUNT; i++, iter++)
		{
			threads.push_back(new std::thread(processBody, sys, *iter, initial[0], 20 - rand() % 10));
		}
		for (auto proc : threads)
		{
			proc->join();
		}
		threads.clear();
		processes.clear();
		ime = ime2;
	}
	running = false;
	pThrad.join();
	std::cout << "\n\nFAULTS: " << fc << "(" << ((long double)fc) / ac * 100 << "%)";
	int a;
	std::cin >> a;
}

#endif