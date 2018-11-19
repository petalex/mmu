#include "Allocator.h"

Allocator::Allocator(BlockNo spaceSize): firstBlock(nullptr), lastBlock(nullptr), spaceSize(spaceSize), size(0) {
	for (unsigned i = 0; i < this->spaceSize; this->put(i++));
}

Allocator::~Allocator() {
	while (this->firstBlock != nullptr)
		this->get();
}

BlockNo Allocator::get() {
	std::lock_guard<std::mutex> lock(mutex);
	if (size == 0) {
		return -1; // no free space
	}
	BlockNode *block = this->firstBlock;
	this->firstBlock = this->firstBlock->next;
	BlockNo blockNo = block->block;
	delete block;
	size--;
	if (size == 0) {
		this->firstBlock = this->lastBlock = nullptr;
	}
	return blockNo;
}

void Allocator::put(BlockNo block) {
	std::lock_guard<std::mutex> lock(mutex);
	if (block >= this->spaceSize) return;
	BlockNode *temp = new BlockNode(block);
	if (this->firstBlock == nullptr) {
		this->firstBlock = temp;
	}
	else {
		this->lastBlock->next = temp;
	}
	this->lastBlock = temp;
	size++;
}