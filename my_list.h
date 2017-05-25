#ifndef __MYLIST_H_
#define __MYLIST_H_

struct linked_list_t;
typedef struct linked_list_t linked_list_t;

typedef struct op_t
{
	int key;
	void* data;
	enum {INSERT, REMOVE, CONTAINS, UPDATE, COMPUTE} op;
	int (*compute_func) (void *);
	int result;
} op_t;

linked_list_t* list_alloc();
void list_free(linked_list_t* list);
int list_split(linked_list_t* list, int n, linked_list_t** arr);
int list_insert(linked_list_t* list, int key, void* data);
int list_remove(linked_list_t* list, int key);
int list_find(linked_list_t* list, int key);
int list_size(linked_list_t* list);
int list_update(linked_list_t* list, int key, void* data);
int list_compute(linked_list_t* list, int key, 
						int (*compute_func) (void *), int* result);
void list_batch(linked_list_t* list, int num_ops, op_t* ops);

#endif /* __MYLIST_ */