#ifndef PROCESS_LIST_H
#define PROCESS_LIST_H

#include "vm_declarations.h"
#include <mutex>

class Process;

typedef struct processNode {
	Process *process;
	struct processNode *next;
	processNode(Process *process, struct processNode* next = nullptr) : process(process), next(next) {}
} ProcessNode;

class ProcessList {
public:
	ProcessList() : firstProcess(nullptr), lastProcess(nullptr) {}
	~ProcessList();
	Process* getProcess(ProcessId pid);
	void putProcess(Process*process);
	void removeProcess(ProcessId pid);
private:
	ProcessNode *firstProcess, *lastProcess;
	std::mutex mutex;
};

#endif