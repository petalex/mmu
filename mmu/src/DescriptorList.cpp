#include "DescriptorList.h"

DescriptorList::~DescriptorList() {
	std::lock_guard<std::mutex> lock(mutex);
	while (this->firstDescriptor) {
		DescriptorNode *temp = this->firstDescriptor;
		this->firstDescriptor = this->firstDescriptor->next;
		delete temp;
	}
	this->lastDescriptor = this->cursor = nullptr;
}

void DescriptorList::resetCursor() {
	std::lock_guard<std::mutex> lock(mutex);
	this->cursor = this->firstDescriptor;
}

bool DescriptorList::end() {
	std::lock_guard<std::mutex> lock(mutex);
	return this->cursor == nullptr;
}

Descriptor* DescriptorList::getNextDescriptor() {
	std::lock_guard<std::mutex> lock(mutex);
	if (!this->cursor) return nullptr;
	DescriptorNode *cur = this->cursor;
	this->cursor = this->cursor->next;
	return cur->descriptor;
}

void DescriptorList::putDescriptor(Descriptor *descriptor) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!descriptor) return;
	this->lastDescriptor = (this->firstDescriptor ? this->lastDescriptor->next : this->firstDescriptor) = new DescriptorNode(descriptor);
}

void DescriptorList::removeDescriptor(Descriptor *descriptor) {
	std::lock_guard<std::mutex> lock(mutex);
	DescriptorNode *cur, *prev;
	for (prev = nullptr, cur = this->firstDescriptor; cur != nullptr && cur->descriptor != descriptor; prev = cur, cur = cur->next);
	if (!cur) return;
	(prev ? prev->next : this->firstDescriptor) = cur->next;
	if (this->cursor == cur) (prev ? prev->next : this->firstDescriptor);
	if (!this->firstDescriptor) this->lastDescriptor = nullptr;
	delete cur;
}