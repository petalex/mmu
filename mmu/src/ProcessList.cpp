#include "ProcessList.h"
#include "Process.h"

ProcessList::~ProcessList() {
	std::lock_guard<std::mutex> lock(mutex);
	while (this->firstProcess) {
		ProcessId pid = this->firstProcess->process->getProcessId();
		delete this->firstProcess->process;
		if (this->getProcess(pid) != nullptr)
			this->removeProcess(pid);
	}
}

Process* ProcessList::getProcess(ProcessId pid) {
	std::lock_guard<std::mutex> lock(mutex);
	ProcessNode *cur;
	for (cur = this->firstProcess; cur != nullptr; cur = cur->next)
		if (cur->process->getProcessId() == pid)
			return cur->process;
	return nullptr;
}

void ProcessList::putProcess(Process *process) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!process) return;
	this->lastProcess = (this->firstProcess ? this->lastProcess->next : this->firstProcess) = new ProcessNode(process);
}

void ProcessList::removeProcess(ProcessId pid) {
	std::lock_guard<std::mutex> lock(mutex);
	ProcessNode *cur, *prev;
	for (prev = nullptr, cur = this->firstProcess; cur != nullptr && cur->process->getProcessId() != pid; prev = cur, cur = cur->next);
	if (!cur) return;
	(prev ? prev->next : this->firstProcess) = cur->next;
	if (!this->firstProcess) this->lastProcess = nullptr;
	delete cur;
}