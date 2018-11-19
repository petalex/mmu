#ifndef DESCRIPTOR_LIST_H
#define DESCRIPTOR_LIST_H

#include "KernelProcess.h"
#include <mutex>

typedef struct descriptorNode {
	Descriptor *descriptor;
	struct descriptorNode *next;
	descriptorNode(Descriptor *descriptor, struct descriptorNode *next = nullptr) : descriptor(descriptor), next(next) {}
} DescriptorNode;

class DescriptorList {
public:
	DescriptorList() : firstDescriptor(nullptr), lastDescriptor(nullptr), cursor(nullptr) {}
	~DescriptorList();
	void resetCursor();
	bool end();
	Descriptor* getNextDescriptor();
	void putDescriptor(Descriptor *descriptor);
	void removeDescriptor(Descriptor *descriptor);
private:
	DescriptorNode *firstDescriptor, *lastDescriptor, *cursor;
	std::mutex mutex;
};

#endif
