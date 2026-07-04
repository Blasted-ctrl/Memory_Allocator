#include <iostream>
#include <Windows.h>
#include <mutex>
#include <cstring>

typedef char ALIGN[16];

// We must know for every block of memory. Is it free and what is the size.
union header{
	struct header_t {
		size_t size;
		unsigned is_free;
		union header *next; // We can see the next header+block of memory with this
	} s;
	ALIGN stub;
};

typedef union header header_t;
header_t* head, *tail;

std::mutex mtx;

header_t* get_free_block(size_t size) {
	header_t* curr = head;
	
	while (curr) {
		if (curr->s.is_free && curr->s.size >= size) {
			return curr;
		}
		curr = curr->s.next;

	}
	return NULL;
}

void* my_malloc(size_t size) {
	size_t totalsize = sizeof(header_t) + size;
	header_t *header;
	if (!size) return NULL;
	// Lock mechanism to disallow multi threads to access this block of memory.
	std::unique_lock<std::mutex> lock(mtx); 

	header = get_free_block(size);

	if (header) {
		header->s.is_free = 0;
		lock.unlock();
		return (void*)(header + 1);
	}

	void* block = VirtualAlloc(NULL, totalsize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	if (block == NULL) {
		lock.unlock();
		return NULL;
	}

	header = (header_t*)block;

	header->s.size = size;
	header->s.is_free = 0;
	header->s.next = NULL;
	if (!head) {
		head = header;
	}
	if (tail) {
		tail->s.next = header;
	}
	tail = header;
	lock.unlock();
	return (void*)(header + 1);
}

void my_free(void* block) {
	header_t* header, * tmp;
	if (!block) {
		return;
	}

	std::unique_lock<std::mutex> lock(mtx);
	header = (header_t*)block - 1;

	header->s.is_free = 1;
}

void* my_calloc(size_t num, size_t nsize) {
	size_t size;
	void* block;
	if (!num || !nsize) {
		return NULL;
	}
	size = num * nsize;
	if (nsize != size / num) return NULL;
	block = my_malloc(size);
	if (!block) return NULL;
	memset(block, 0, size);
		return block;
}

void* my_realloc(void* block, size_t size) {
	header_t* header;
	void* ret;
	if (!block || !size) {
		return my_malloc(size);
	}
	header = (header_t*)block - 1;
	if (header->s.size >= size) {
		return block;
	}
	ret = my_malloc(size);
	if (ret) {
		memcpy(ret, block, header->s.size);
		my_free(block);
	}
	return ret;
}

int main() {
	int* arr = (int*)my_malloc(3 * sizeof(int));

	arr[0] = 1;
	arr[1] = 2;
	arr[2] = 3;

	arr = (int*)my_realloc(arr, 6 * sizeof(int));

	std::cout << arr[0] << " "
		<< arr[1] << " "
		<< arr[2] << "\n";
}
