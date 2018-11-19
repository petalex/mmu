#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "vm_declarations.h"
#include <mutex>

typedef unsigned long BlockNo;

typedef struct blockNode {
	BlockNo block;
	struct blockNode *next;
	blockNode(BlockNo block, struct blockNode* next = nullptr) : block(block), next(nullptr) {}
} BlockNode;

class Allocator {
public:
	Allocator(BlockNo spaceSize);
	~Allocator();
	BlockNo allocate() {
		return this->get();
	}
	void deallocate(BlockNo block) {
		this->put(block);
	}
	bool noSpace(unsigned size = 1) {
		std::lock_guard<std::mutex> lock(mutex);
		return (this->size < size);
	}
protected:
	BlockNo get();
	void put(BlockNo block);
public:
	BlockNode *firstBlock, *lastBlock;
	BlockNo spaceSize;
	unsigned size;
	std::mutex mutex;
};

#endif