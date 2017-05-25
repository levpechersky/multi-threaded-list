/*
 * my_list_test.c
 *
 *  Created on: 25 May 2017
 *      Author: Lev
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "my_list.h"

#define ASSERT_TEST(b) do { \
        if (!(b)) { \
                fprintf(stdout, "\nAssertion failed at %s:%d %s ",__FILE__,__LINE__,#b); \
                return false; \
        } \
} while (0)

#define RUN_TEST(test) do { \
        fprintf(stdout, "Testing "#test"... "); \
        if (test()) { \
            fprintf(stdout, "[OK]\n");\
        } else { \
        	fprintf(stdout, "[Failed]\n"); \
        } \
} while(0)

/*

 int list_split(linked_list_t* list, int n, linked_list_t** arr);
 int list_update(linked_list_t* list, int key, void* data);
 int list_compute(linked_list_t* list, int key,
 void list_batch(linked_list_t* list, int num_ops, op_t* ops);


 */
bool alloc_and_free() {
	linked_list_t* list = list_alloc();
	ASSERT_TEST(list!=NULL);
	list_free(NULL); // should not fail
	list_free(list);
	return true;
}

bool insert_empty_list() {
	int key = 42, data = 4;
	linked_list_t* list = list_alloc();
	list_insert(list, key, &data);

	list_free(list);
	return true;
}

bool insert_sorted_first() {
	int key_big = 20, data_big = 2;
	int key_small = 10, data_small = 1;
	linked_list_t* list = list_alloc();

	ASSERT_TEST(list_insert(list, key_big, &data_big) == 0);
	ASSERT_TEST(list_insert(list, key_small, &data_small) == 0);
	//TODO check order

	list_free(list);
	return true;
}

bool insert_sorted_end() {
	int key_big = 20, data_big = 2;
	int key_small = 10, data_small = 1;
	linked_list_t* list = list_alloc();

	ASSERT_TEST(list_insert(list, key_small, &data_small) == 0);
	ASSERT_TEST(list_insert(list, key_big, &data_big) == 0);
	//TODO check order

	list_free(list);
	return true;
}

bool insert_sorted_middle() {
	int key_big = 30, data_big = 3;
	int key_mid = 20, data_mid = 2;
	int key_small = 10, data_small = 1;
	linked_list_t* list = list_alloc();

	ASSERT_TEST(list_insert(list, key_small, &data_small) == 0);
	ASSERT_TEST(list_insert(list, key_big, &data_big) == 0);
	ASSERT_TEST(list_insert(list, key_mid, &data_mid) == 0);
	//TODO check order

	list_free(list);
	return true;
}

bool find_empty_list() {
	linked_list_t* list = list_alloc();

	ASSERT_TEST(list_find(list, 1024) == 0);
	ASSERT_TEST(list_find(list, 0) == 0);
	ASSERT_TEST(list_find(list, -1024) == 0);

	list_free(list);
	return true;
}

bool find_nonempty_list() {
	int key_big = 30, data_big = 3;
	int key_mid = 20, data_mid = 2;
	int key_small = 10, data_small = 1;
	linked_list_t* list = list_alloc();
	list_insert(list, key_small, &data_small);
	list_insert(list, key_big, &data_big);
	list_insert(list, key_mid, &data_mid);

	ASSERT_TEST(list_find(list, key_big) == 1);
	ASSERT_TEST(list_find(list, key_mid) == 1);
	ASSERT_TEST(list_find(list, key_small) == 1);
	ASSERT_TEST(list_find(list, 1024) == 0);
	ASSERT_TEST(list_find(list, 0) == 0);
	ASSERT_TEST(list_find(list, -1024) == 0);

	list_free(list);
	return true;
}

bool size() {
	int keys[] = { -29219, -24086, -10898, -6117, 2177, 11394, 11737, 16425,
			17654, 27198 };
	int keys_n = 10;
	linked_list_t* list = list_alloc();
	for (int i = 0; i < keys_n; i++) {
		list_insert(list, keys[i], &keys[i]);
		ASSERT_TEST(list_size(list) == i + 1);
	}
	list_free(list);
	return true;
}

bool remove_success() {
	int keys[] = { -29219, -24086, -10898, -6117, 2177, 11394, 11737, 16425,
			17654, 27198 };
	int keys_n = 10;
	linked_list_t* list = list_alloc();
	for (int i = 0; i < keys_n; i++) {
		list_insert(list, keys[i], &keys[i]);
	}
	for (int i = 0; i < keys_n; i++) {
		ASSERT_TEST(list_remove(list, keys[i]) == 0);
	}
	list_free(list);
	return true;
}

bool remove_item_not_in_list() {
	int keys[] = { -29219, -24086, -10898, -6117, 2177, 11394, 11737, 16425,
			17654, 27198 };
	int not_in_list[] = { -1000000, -15000, -3000, 0, 2, 100, 15000, 1000000 };
	int keys_n = 10, not_in_list_n = 8;
	linked_list_t* list = list_alloc();
	for (int i = 0; i < keys_n; i++) {
		list_insert(list, keys[i], &keys[i]);
	}
	for (int i = 0; i < not_in_list_n; i++) {
		ASSERT_TEST(list_remove(list, not_in_list[i]) != 0);
	}
	list_free(list);
	return true;
}

int main(int argc, char** argv) {
	RUN_TEST(alloc_and_free);
	RUN_TEST(insert_empty_list);
	RUN_TEST(insert_sorted_first);
	RUN_TEST(insert_sorted_end);
	RUN_TEST(insert_sorted_middle);
	RUN_TEST(find_empty_list);
	RUN_TEST(find_nonempty_list);
	RUN_TEST(size);
	RUN_TEST(remove_success);
	RUN_TEST(remove_item_not_in_list);
	return 0;
}

