#include "sort.h"

//merge sort based on: https://www.geeksforgeeks.org/merge-sort-for-linked-list/

struct sort_node *sorted_merge(struct sort_node *a, struct sort_node *b, struct sort_node *(*comparator)(void *a, void *b)) { 
	struct sort_node* result = NULL; 

	/* Base cases */
	if (a == NULL) {
		return (b);
	} else if (b == NULL) { 
		return (a);
	}

	/* Pick either a or b, and recur */
	result = comparator((void *)a,(void *)b);
	if(result == a) {
		result->next = sorted_merge(a->next, b, comparator);
	} else {
		result->next = sorted_merge(a, b->next, comparator);
	}
	return (result);
}

void front_back_split(struct sort_node *source, struct sort_node **front_ref, struct sort_node **back_ref) {
	struct sort_node *fast;
	struct sort_node *slow;
	slow = source;
	fast = source->next;

	/* Advance 'fast' two nodes, and advance 'slow' one node */
	while (fast != NULL) {
		fast = fast->next;
		if (fast != NULL) {
			slow = slow->next;
			fast = fast->next;
		}
	}

	/* 'slow' is before the midpoint in the list, so split it in two at that point. */
	*front_ref = source;
	*back_ref = slow->next;
	slow->next = NULL;
}

void merge_sort(struct sort_node **head_ref, struct sort_node *(*comparator)(void *a, void *b)) {
    struct sort_node *head = *head_ref;
    struct sort_node *a;
    struct sort_node *b;

    /* Base case -- length 0 or 1 */
    if ((head == NULL) || (head->next == NULL)) {
        return;
    }

    /* Split head into 'a' and 'b' sublists */
    front_back_split(head, &a, &b);

    /* Recursively sort the sublists */
    merge_sort(&a, comparator);
    merge_sort(&b, comparator);

    /* answer = merge the two sorted lists together */
    *head_ref = sorted_merge(a, b, comparator);
}
