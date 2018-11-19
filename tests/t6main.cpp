#include "testdefs.h"


#ifdef _VM_TEST_6

/*	"Pratece napomene":
* 1. Ne testira se trashing, niti sinhronizacija niti.
* 2. Ukoliko se koristi nit za cuvanje podataka na disku u pozadini, treba
*    dodati metodu 'std::mutex* getPoolSynchronizer()' u okviru klase System
*    koja vraca mutex koji se koristi za sinhronizaciju ove niti i takodje
*    treba definisati konstantu POOL_SYNC (pogledati ispod #define); inace,
*    treba obrisati ovaj #define (meni cak radi i u varijanti kada pustim tu
*    nit bez da je sinhronizujem u testu - izbrisem POOL_SYNC iako ostavim
*    tu nit u pozadini da radi -> zavisi od vise stvari).
* 3. Testovi za kloniranje su oznaceni sa [CT].
* 4. Testovi za share-ovanje su oznaceni sa [ST].
* 5. U Status enum dodati na kraj AccessViolation.
* 6. Kod kreiranja/brisanja segmenata, ne-nulti broj oznacava bilo kakvu
*    gresku, dok 0 oznacava ok.
* 7. Test ne radi periodicJob.
*/

#include <iostream>
#include <iomanip>
#include <Windows.h>
#include <cstdio>
#include <chrono>
#include <mutex>
#include "vm_declarations.h"
#include "part.h"
#include "system.h"
#include "process.h"
#include <fstream>
using namespace std::chrono;
using std::mutex;
//using std::cout;
using std::endl;

std::ofstream cout("t6dump.txt");

//#define POOL_SYNC

typedef unsigned long Size;
typedef unsigned long ulong;
typedef unsigned char byte;

bool hit(System* system, Process* proc, VirtualAddress address, mutex* mtx, byte* data, bool execute)
{
	AccessType accessType = execute ? EXECUTE : (data ? WRITE : READ);
	PhysicalAddress adr = nullptr;
	bool pageFault = false;
	Status status = Status::TRAP;

	if (mtx) mtx->lock();
	cout << std::hex;
	cout << "\t==> PID[" << std::dec << std::setfill('0') << std::setw(5) << proc->getProcessId() << std::hex << "] VirtualAddress 0x" << std::setfill('0') << std::setw(6) << address << " [" << std::dec << std::setfill('0') << std::setw(4) << (address / PAGE_SIZE) << std::hex << "]" << " -> ";
	if ((status = system->access(proc->getProcessId(), address, accessType)) == Status::PAGE_FAULT)
	{
		pageFault = true;
		cout << "Page fault." << endl;
		if (mtx) mtx->unlock();
		proc->pageFault(address);
		if (mtx) mtx->lock();
		cout << "\t==> PID[" << std::setfill('0') << std::setw(5) << std::dec << proc->getProcessId() << std::hex << "] VirtualAddress 0x" << std::setfill('0') << std::setw(6) << address << " [" << std::dec << std::setfill('0') << std::setw(4) << (address / PAGE_SIZE) << std::hex << "]" << " -> ";
		status = system->access(proc->getProcessId(), address, accessType);
	}
	else if (status == Status::TRAP) cout << "Trap" << endl;
	else if (status == Status::ACCESS_VIOLATION) cout << "Access violation." << endl;

	if (status == Status::OK)
	{
		adr = proc->getPhysicalAddress(address);
		if (data) cout << "PhysicalAddress 0x" << std::setfill('0') << std::setw(8) << (unsigned long long)adr << " WR [0x" << std::setfill('0') << std::setw(2) << (unsigned int)(*((byte*)adr) = *data) << "]" << endl;
		else if (execute) cout << "PhysicalAddress 0x" << std::setfill('0') << std::setw(8) << (unsigned long long)adr << " EX [0x" << std::setfill('0') << std::setw(2) << (unsigned int)*((byte*)adr) << "]" << endl;
		else cout << "PhysicalAddress 0x" << std::setfill('0') << std::setw(8) << (unsigned long long)adr << " RD [0x" << std::setfill('0') << std::setw(2) << (unsigned int)*((byte*)adr) << "]" << endl;
	}
	else if (status == Status::PAGE_FAULT) cout << "INVALID PAGE FAULT!" << endl;

	cout << std::dec;
	if (mtx) mtx->unlock();

	return pageFault;
}
bool hit(System* system, Process* proc, VirtualAddress address, mutex* mtx, byte* data)
{
	return hit(system, proc, address, mtx, data, false);
}

void run()
{
	struct Access
	{
		System* system;
		Process* process;
		VirtualAddress address;
		Size count;
		AccessType type;
		Access(System* system = nullptr, Process* process = nullptr, VirtualAddress address = 0, Size count = 0, AccessType type = AccessType::READ) : system(system), process(process), address(address), count(count), type(type) { }
	};

	static const PageNum processVMSpaceSize = 10;
	static const PageNum pmtSpaceSize = 15;

	byte* processVMSpace = new byte[PAGE_SIZE * processVMSpaceSize];
	byte* pmtSpace = new byte[PAGE_SIZE * pmtSpaceSize];

	cout << "Loading partition . . ." << endl;
	Partition* part = new Partition("p1.ini");
	cout << "Complete!" << endl;

	cout << "Creating system . . ." << endl;
	System* system = new System(processVMSpace, processVMSpaceSize, pmtSpace, pmtSpaceSize, part);
	cout << "Created!" << endl;

	cout << "Creating processes . . ." << endl;
	Process* proc0 = system->createProcess();
	Process* proc1 = system->createProcess();
	Process* proc2 = system->createProcess();
	Process* proc0c1 = nullptr;
	Process* proc0c2 = nullptr;
	Process* proc0c2c1 = nullptr;
	cout << "Created!" << endl;

	cout << "Creting segments . . ." << endl;
	cout << proc0->createSegment(0, 5, AccessType::READ_WRITE) << endl;				// 0
	cout << proc1->createSegment(0, 3, AccessType::READ_WRITE) << endl;				// 0
	cout << proc2->createSegment(0, 3, AccessType::EXECUTE) << endl;				// 0
	cout << "Created!" << endl;

	cout << "Creating false segments . . ." << endl;
	cout << proc0->createSegment(5121, 4, AccessType::READ_WRITE) << endl;			// 2
	cout << proc0->createSegment(4096, 4, AccessType::READ_WRITE) << endl;			// 3
	cout << proc0->createSegment(5120, 6, AccessType::READ_WRITE) << endl;			// 0
	cout << proc0->createSegment(8192, 3, AccessType::READ_WRITE) << endl;			// 3
	cout << proc0->deleteSegment(5120) << endl;										// 0
	cout << proc0->createSegment(8192, 3, AccessType::READ_WRITE) << endl;			// 0
	cout << proc0->deleteSegment(5120) << endl;										// 2
	cout << proc0->deleteSegment(8192) << endl;										// 0
	cout << proc0->createSegment(0x010000, 577, AccessType::READ_WRITE) << endl;	// 2
	cout << proc0->deleteSegment(0x010000) << endl;									// 2
	cout << proc0->createSegment(0x010000, 576, AccessType::READ_WRITE) << endl;	// 0
	cout << proc0->createSegment(0xFFFC00, 1, AccessType::READ_WRITE) << endl;		// 2
	cout << proc0->deleteSegment(0xFFFC00) << endl;									// 2
	cout << proc0->deleteSegment(0x010000) << endl;									// 0
	cout << proc0->createSegment(0xFFFC00, 1, AccessType::READ_WRITE) << endl;		// 0
	cout << proc0->deleteSegment(0xFFFC00) << endl;									// 0
	cout << proc0->createSegment(0xFFFC00, 2, AccessType::READ_WRITE) << endl;		// 2
	cout << "Done!" << endl;

	cout << "Creating shared segments . . ." << endl;
	cout << proc0->createSharedSegment(5120, 3, "burek", AccessType::READ_WRITE) << endl;	// 0
	cout << proc1->createSharedSegment(3072, 3, "burek", AccessType::READ_WRITE) << endl;	// 0
	cout << proc2->createSharedSegment(3072, 2, "burek", AccessType::READ_WRITE) << endl;	// 2
	cout << proc2->createSharedSegment(3072, 3, "burek", AccessType::READ_WRITE) << endl;	// 0
	cout << proc2->disconnectSharedSegment("burek") << endl;								// 0
	cout << proc2->disconnectSharedSegment("burek") << endl;								// 2
	cout << proc2->createSegment(3072, 2, AccessType::READ_WRITE) << endl;					// 0
	cout << proc2->deleteSegment(3072) << endl;												// 0
	cout << "Done!" << endl;

	static const Size accessSize = 15;
	Access* access = new Access[accessSize];
	unsigned long pageFaults = 0; byte data;
	mutex* mtx = nullptr;

#ifdef POOL_SYNC
	system->getPoolSynchronizer();
#endif

	access[0] = Access(system, proc0, 0, 100);							// 0.page 0
	access[1] = Access(system, proc0, 4096, 50, AccessType::READ_WRITE);// 0.page 4
	access[2] = Access(system, proc0, 2048, 100);							// 0.page 2
	access[3] = Access(system, proc0, 1020, 50);							// 0.page 0-1

	access[4] = Access(system, proc1, 0, 100);							// 1.page 0
	access[5] = Access(system, proc1, 2048, 100);							// 1.page 2
	access[6] = Access(system, proc1, 1020, 50);							// 1.page 0-1

	access[7] = Access(system, proc2, 0, 100, AccessType::EXECUTE);	// 2.page 0
	access[8] = Access(system, proc2, 1020, 50, AccessType::EXECUTE);	// 2.page 0-1
	access[9] = Access(system, proc2, 2040, 50, AccessType::EXECUTE);	// 2.page 1-2;

	access[10] = Access(system, proc0, 1020, 20, AccessType::EXECUTE);	// ACCESS VIOLATION
	access[11] = Access(system, proc0, 80190, 20);							// TRAP
	access[12] = Access(system, proc0, 16380, 20, AccessType::EXECUTE);	// TRAP
	access[13] = Access(system, proc0, 3072, 50);							// 0.page 3
	access[14] = Access(system, proc0, 4090, 100);							// 0.page 3-4

	cout << "Accessing . . ." << endl;

	high_resolution_clock::time_point t1 = high_resolution_clock::now();

	proc0c1 = system->cloneProcess(proc0->getProcessId());

	cout << endl;

	cout << "[CT]"; pageFaults += hit(system, proc0, 0, mtx, nullptr);			// [CT] PHADR1 RD [0x00]	- garbage podatak 1
	cout << "[CT]"; pageFaults += hit(system, proc0, 0, mtx, &(data = 0xDA));	// [CT] PHADR2 WR [0xda]
	cout << "[ST]"; pageFaults += hit(system, proc0c1, 5120, mtx, nullptr);			// [ST] PHADR3 RD [0x00]	- garbage podatak 2
	cout << "[ST]"; pageFaults += hit(system, proc0, 5120, mtx, nullptr);			// [ST] PHADR3 RD [0x00]	- garbage podatak 2
	cout << "[ST]"; pageFaults += hit(system, proc1, 3072, mtx, &(data = 0xDE));	// [ST] PHADR3 WR [0xde]

	cout << endl;

	for (ulong a = 0; a < accessSize; a++)
		for (VirtualAddress i = access[a].address; i < access[a].address + access[a].count; i++)
			pageFaults += hit(access[a].system, access[a].process, i, mtx, access[a].type == WRITE || access[a].type == READ_WRITE ? &(data = 0x0) : nullptr, access[a].type == EXECUTE);

	proc0c2 = system->cloneProcess(proc0->getProcessId());
	proc0c2c1 = system->cloneProcess(proc0c2->getProcessId());

	cout << endl;

	cout << "[CT]"; pageFaults += hit(system, proc0c1, 0, mtx, nullptr);		// [CT] PHADR4 RD [0x0]		- garbage podatak 1
	cout << "[CT]"; pageFaults += hit(system, proc0c2, 0, mtx, nullptr);		// [CT] PHADR5 RD [0xda]
	cout << "[CT]"; pageFaults += hit(system, proc0c2c1, 0, mtx, nullptr);		// [CT] PHADR5 RD [0xda]
	cout << "[ST]"; pageFaults += hit(system, proc0c2, 5120, mtx, nullptr);		// [ST] PHADR6 RD [0xde]
	cout << "[ST]"; pageFaults += hit(system, proc0c2c1, 5120, mtx, nullptr);		// [ST] PHADR6 RD [0xde]
	cout << "[ST]"; pageFaults += hit(system, proc0, 5120, mtx, nullptr);		// [ST] PHADR6 RD [0xde]
	cout << "[ST]"; pageFaults += hit(system, proc1, 3072, mtx, nullptr);		// [ST] PHADR6 RD [0xde]
	cout << "[ST]"; pageFaults += hit(system, proc1, 3072, mtx, &(data = 0x0));// [ST] PHADR6 WR [0x0]

	cout << endl;

	for (ulong a = 0; a < accessSize; a++)
		for (VirtualAddress i = access[a].address; i < access[a].address + access[a].count; i++)
			pageFaults += hit(access[a].system, access[a].process, i, mtx, access[a].type == WRITE || access[a].type == READ_WRITE ? &(data = 0x0) : nullptr, access[a].type == EXECUTE);

	cout << endl;

	cout << "[CT]"; pageFaults += hit(system, proc0, 0, mtx, nullptr);		// [CT] PHADR7 RD [0xda] - ZAVISNO OD IMPLEMENTACIJE CoW, ako se kopiranje vrsi nad stranicom, PHADR7 je isto kao PHADR8
	cout << "[CT]"; pageFaults += hit(system, proc0c2, 0, mtx, nullptr);		// [CT] PHADR8 RD [0xda]
	cout << "[CT]"; pageFaults += hit(system, proc0c2c1, 0, mtx, nullptr);		// [CT] PHADR8 RD [0xda]
	cout << "[ST]"; pageFaults += hit(system, proc0c1, 5120, mtx, nullptr);		// [ST] PHADR9 RD [0x0]
	cout << "[ST]"; pageFaults += hit(system, proc0c2, 5120, mtx, nullptr);		// [ST] PHADR9 RD [0x0]

	cout << endl;

	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	duration<double> time_span = duration_cast<duration<double>>(t2 - t1);

	cout << "Done in " << time_span.count() << "s with " << std::dec << pageFaults << " page fault(s)!" << endl;

	delete[] access;

	cout << "Deleting segments . . ." << endl;
	cout << proc0->deleteSharedSegment("burek") << endl;					// 0
	cout << proc1->deleteSharedSegment("burek") << endl;					// 2
	cout << proc1->createSegment(3072, 3, AccessType::READ_WRITE) << endl;	// 0
	cout << proc1->deleteSegment(3072) << endl;								// 0
	cout << proc0->deleteSegment(0) << endl;								// 0
	cout << proc1->deleteSegment(0) << endl;								// 0
	cout << proc2->deleteSegment(0) << endl;								// 0
	cout << "Done!" << endl;

	cout << "Deleting processes . . ." << endl;
	delete proc0;
	delete proc1;
	delete proc2;
	delete proc0c1;
	delete proc0c2;
	delete proc0c2c1;
	cout << "Done!" << endl;

	cout << "Deleting system . . ." << endl;
	delete system;
	cout << "Done!" << endl;

	cout << "Deleting partition . . ." << endl;
	delete part;
	cout << "Done!" << endl;

	cout << "Deleting pmtSpace . . ." << endl;
	delete[] pmtSpace;
	cout << "Done!" << endl;

	cout << "Deleting processVMSpace . . ." << endl;
	delete[] processVMSpace;
	cout << "Done!" << endl;
}

int main(void)
{
	SetConsoleOutputCP(CP_UTF8);
	setvbuf(stdout, nullptr, _IONBF, 1000);
	std::thread t(run);
	t.join();
	cout << endl;
	system("pause");
	return 0;
}

#endif