#include "testdefs.h"

#ifdef _VM_TEST_7

#include <iostream>

#include <unordered_map>
#include "vm_declarations.h"
#include <memory>
#include "Process.h"
#include <thread>         // std::this_thread::sleep_until
#include "System.h"

#include <mutex>
#include <part.h>

using namespace std;

class Dummy1 {
	char niz[256 * 1024];
};

class Dummy2 {
	char niz[257 * 1024];
};

class Dummy {
	char niz[1024];
};

Dummy1* dm1 = new Dummy1();
Dummy2* dm2 = new Dummy2();
Partition* par = new Partition("p1.ini");
System* sys = new System(dm1, 1, dm2, 257, par);

void main() {

	//cout << "Broj PMT blokova = "; KernelSystem::allocPMTSpace->pisi(); cout << endl;
	//cout << "Broj PRO blokova = "; KernelSystem::allocProcSpace->pisi(); cout << endl;
	//cout << "Broj DSC blokova = "; KernelSystem::dscAlloc->pisi(); cout << endl;

	Process* p0 = sys->createProcess();
	cout << "Create = " << p0->createSegment(0x300'C00, 100, READ_WRITE) << endl;
	cout << "Access = " << p0->getProcessId() << "  " << sys->access(p0->getProcessId(), 0x300'C00, READ) << endl;
	cout << "Page F = " << p0->getProcessId() << "  " << p0->pageFault(0x300'C00) << endl;
	cout << endl;

	Process* p1 = sys->cloneProcess(p0->getProcessId());
	cout << "Create = " << p1->createSharedSegment(0x200'C00, 64, "Jovan", READ_WRITE) << endl;
	cout << "Access = " << p1->getProcessId() << "  " << sys->access(p1->getProcessId(), 0x200'C00, READ) << endl;
	cout << "Page F = " << p1->getProcessId() << "  " << p1->pageFault(0x200'C00) << endl;
	cout << endl;

	cout << "Create = " << p0->createSharedSegment(0x100'C00, 64, "Jovan", READ_WRITE) << endl;
	cout << "Access = " << p0->getProcessId() << "  " << sys->access(p0->getProcessId(), 0x100'C00, READ) << endl;
	cout << "Access = " << p0->getProcessId() << "  " << sys->access(p0->getProcessId(), 0x300'C00, READ) << endl;
	cout << "Access = " << p1->getProcessId() << "  " << sys->access(p1->getProcessId(), 0x300'C00, READ) << endl;
	cout << "Page F = " << p0->getProcessId() << "  " << p0->pageFault(0x300'C00) << endl;
	cout << "Access = " << p0->getProcessId() << "  " << sys->access(p0->getProcessId(), 0x300'C00, READ) << endl;
	cout << "Access = " << p1->getProcessId() << "  " << sys->access(p1->getProcessId(), 0x300'C00, READ) << endl;
	cout << endl;

	Process* p2 = sys->cloneProcess(p0->getProcessId());
	cout << "Access = " << p2->getProcessId() << "  " << sys->access(p2->getProcessId(), 0x300'C00, READ) << endl;
	cout << "Access = " << p2->getProcessId() << "  " << sys->access(p2->getProcessId(), 0x200'C00, READ) << endl;
	cout << "Access = " << p2->getProcessId() << "  " << sys->access(p2->getProcessId(), 0x100'C00, READ) << endl;
	cout << "Page F = " << p2->getProcessId() << "  " << p2->pageFault(0x300'C00) << endl; // Edge case lvl Tartalja :)
	cout << endl;

	Process* p3 = sys->cloneProcess(p2->getProcessId());
	cout << "Access = " << p3->getProcessId() << "  " << sys->access(p3->getProcessId(), 0x300'C00, READ) << endl;
	cout << "Access = " << p2->getProcessId() << "  " << sys->access(p2->getProcessId(), 0x300'C00, READ) << endl;
	cout << "Access = " << p1->getProcessId() << "  " << sys->access(p1->getProcessId(), 0x300'C00, READ) << endl;
	cout << "Access = " << p0->getProcessId() << "  " << sys->access(p0->getProcessId(), 0x300'C00, READ) << endl;
	cout << endl;

	cout << "Page F = " << p3->getProcessId() << "  " << p3->pageFault(0x100'C00) << endl;
	cout << "Access = " << p3->getProcessId() << "  " << sys->access(p3->getProcessId(), 0x100'C00, READ) << endl;
	cout << "Access = " << p2->getProcessId() << "  " << sys->access(p2->getProcessId(), 0x100'C00, READ) << endl;
	cout << "Access = " << p1->getProcessId() << "  " << sys->access(p1->getProcessId(), 0x200'C00, READ) << endl;
	cout << "Access = " << p0->getProcessId() << "  " << sys->access(p0->getProcessId(), 0x100'C00, READ) << endl;
	cout << endl;

	cout << "Page F = " << p3->getProcessId() << "  " << p3->pageFault(0x300'C00) << endl;
	cout << "Delete = " << p3->getProcessId() << "  " << p3->deleteSegment(0x300'C00) << endl;
	//cout << "Broj PRO blokova = "; KernelSystem::allocProcSpace->pisi(); cout << endl; // Ovde proveriti broj blokova u OM.
	//cout << "Broj DSC blokova = "; KernelSystem::dscAlloc->pisi(); // Ovde proveriti broj kls. na disku.
	cout << "Access = " << p2->getProcessId() << "  " << sys->access(p2->getProcessId(), 0x300'C00, READ) << endl;
	cout << "Delete = " << p2->getProcessId() << "  " << p2->deleteSegment(0x300'C00) << endl;
	//cout << "Broj PRO blokova = "; KernelSystem::allocProcSpace->pisi(); cout << endl; // Ovde proveriti broj blokova u OM.
	//cout << "Broj DSC blokova = "; KernelSystem::dscAlloc->pisi(); cout << endl;  // Ovde proveriti broj kls. na disku.
	cout << endl;

	cout << "Delete = " << p1->getProcessId() << "  " << p1->deleteSegment(0x300'C00) << endl;
	//cout << "Broj PRO blokova = "; KernelSystem::allocProcSpace->pisi(); cout << endl; // Ovde proveriti broj blokova u OM.
	//cout << "Broj DSC blokova = "; KernelSystem::dscAlloc->pisi();  // Ovde proveriti broj kls. na disku.
	cout << "Access = " << p0->getProcessId() << "  " << sys->access(p0->getProcessId(), 0x300'C00, READ) << endl;
	cout << "Delete = " << p0->getProcessId() << "  " << p0->deleteSegment(0x300'C00) << endl;
	//cout << "Broj PRO blokova = "; KernelSystem::allocProcSpace->pisi(); cout << endl; // Ovde proveriti broj blokova u OM.
	//cout << "Broj DSC blokova = "; KernelSystem::dscAlloc->pisi(); cout << endl;  // Ovde proveriti broj kls. na disku.
	cout << endl;

	cout << "DeletS = " << p1->deleteSharedSegment("Jovan") << endl;
	cout << "Access = " << p3->getProcessId() << "  " << sys->access(p3->getProcessId(), 0x100'C00, READ) << endl;
	cout << "Access = " << p2->getProcessId() << "  " << sys->access(p2->getProcessId(), 0x100'C00, READ) << endl;
	cout << "Access = " << p1->getProcessId() << "  " << sys->access(p1->getProcessId(), 0x200'C00, READ) << endl;
	cout << "Access = " << p0->getProcessId() << "  " << sys->access(p0->getProcessId(), 0x100'C00, READ) << endl;

	delete sys;
	//cout << "Broj PMT blokova = "; KernelSystem::allocPMTSpace->pisi(); cout << endl; // Ovde proveriti broj blokova u OM.
	//cout << "Broj PRO blokova = "; KernelSystem::allocProcSpace->pisi(); cout << endl; // Ovde proveriti broj blokova u OM.
	cout << "Vucicu ...!" << endl;
}

#endif