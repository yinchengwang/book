/* Redis's API: a generic doubly linked list implementation */

#include <stdlib.h>

#include "redis/redis_adlist.h"

/* create a new list, list can be freed with redis_list_release()
 * private value of every node need to be freed by setting a free method using LIST_SET_FREE_METHOD
 */
redis_dlist_t *redis_list_create()
{
    redis_dlist_t *list = NULL;

    if ((list = malloc(sizeof(redis_dlist_t))) == NULL) {
        return NULL;
    }

    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;

    return list;
}

/* Free the whole list. This function can't fail. */
void redis_list_release(redis_dlist_t *list)
{
    if (list == NULL) {
        return;
    }

    redis_list_empty(list);

    free(list);
}

/* Generic version of redis_list_release. */
void redis_list_release_generic(void *list)
{
    redis_list_release((redis_dlist_t *)list);
}

/* Remove all the elements from the list without destroying the list itself. */
void redis_list_empty(redis_dlist_t *list)
{
    if (list == NULL) {
        return;
    }

    unsigned long len = list->len;
    redis_dlist_node_t *curr = list->head;
    redis_dlist_node_t *next = NULL;

    while (len--) {
        next = curr->next;
        if (list->free) {
            list->free(curr->val);
        }
        free(curr);
        curr = next;
    }
    
    list->head = list->tail = NULL;
    list->len = 0;
}

/* Add a node that has already been allocated to the head of list */
void redis_list_link_node_head(redis_dlist_t *rlist, redis_dlist_node_t *node)
{
    if (rlist->len == 0) {
        rlist->head = rlist->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->prev = NULL;
        node->next = rlist->head;
        rlist->head->prev = node;
        rlist->head = node;
    }

    rlist->len++;
}

/* Add a node that has already been allocated to the tail of list */
void redis_list_link_node_tail(redis_dlist_t *rlist, redis_dlist_node_t *node)
{
    if (rlist->len == 0) {
        rlist->head = rlist->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->prev = rlist->tail;
        node->next = NULL;
        rlist->tail->next = node;
        rlist->tail = node;
    }

    rlist->len++;
}

/* Remove the specified node from the list without freeing it. */
void redis_list_unlink_node(redis_dlist_t *rlist, redis_dlist_node_t *node)
{
    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        rlist->head = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else {
        rlist->tail = node->prev;
    }

    rlist->len--;
}

void redis_list_del_node(redis_dlist_t *rlist, redis_dlist_node_t *node)
{
    redis_list_unlink_node(rlist, node);
    if (rlist->free) {
        rlist->free(node->val);
    }
    free(node);
}

redis_dlist_t *redis_list_add_node_head(redis_dlist_t *rlist, void *value)
{
    redis_dlist_node_t *new_node = malloc(sizeof(redis_dlist_node_t));
    if (new_node == NULL) {
        return NULL;
    }

    redis_list_link_node_head(rlist, new_node);
    new_node->val = value;

    return rlist;
}

redis_dlist_t *redis_list_add_node_tail(redis_dlist_t *rlist, void *value)
{
    redis_dlist_node_t *new_node = malloc(sizeof(redis_dlist_node_t));
    if (new_node == NULL) {
        return NULL;
    }

    redis_list_link_node_tail(rlist, new_node);
    new_node->val = value;

    return rlist;
}

/* Inserts elements before or after the specified node in the linked list */
redis_dlist_t *redis_list_insert_node(redis_dlist_t *rlist, redis_dlist_node_t *old_node, void *value, bool after)
{
    redis_dlist_node_t *new_node = malloc(sizeof(redis_dlist_node_t));
    if (new_node == NULL) {
        /* Function is required to return list, return null to determine whether the insertion was successful? */
        return NULL;
    }
    new_node->val = value;

    /* Determines whether to insert in front or behind the specified node */
    if (after) {
        new_node->prev = old_node;
        new_node->next = old_node->next;

        if (old_node == rlist->tail) {
            rlist->tail = new_node;
        }
    } else {
        new_node->next = old_node;
        new_node->prev = old_node->prev;

        if (old_node == rlist->head) {
            rlist->head = new_node;
        }
    }

    /* Adjusting Neighbor nodes */
    if (new_node->prev != NULL) {
        new_node->prev->next = new_node;
    }

    if (new_node->next != NULL) {
        new_node->next->prev = new_node;
    }

    rlist->len++;
    return rlist;
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to redis_list_next() will return the next element of the list.
 */
redis_list_iter_t *redis_list_get_iter(redis_dlist_t *rlist, redis_list_iter_dir_e direction)
{
    redis_list_iter_t *iter = (redis_list_iter_t *)malloc(sizeof(redis_list_iter_t));
    if (iter == NULL) {
        return NULL;
    }

    if (direction == AL_START_HEAD) {
        iter->next = rlist->head;
    } else {
        iter->next = rlist->tail;
    }
    iter->direction = direction;

    return iter;
}

void redis_list_release_iter(redis_list_iter_t *iter)
{
    if (iter == NULL) {
        return;
    }

    free(iter);
}

redis_dlist_node_t *redis_list_next(redis_list_iter_t *iter)
{
    redis_dlist_node_t *current = iter->next;
    if (current != NULL) {
        if (iter->direction == AL_START_HEAD) {
            iter->next = current->next;
        } else {
            iter->next = current->prev;
        }
    }

    return current;
}

void redis_list_iter_rewind(redis_dlist_t *rlist, redis_list_iter_t *iter)
{
    iter->next = rlist->head;
    iter->direction = AL_START_HEAD;
}

void redis_list_iter_rewind_tail(redis_dlist_t *rlist, redis_list_iter_t *iter)
{
    iter->next = rlist->tail;
    iter->direction = AL_START_TAIL;
}

/* Rotate the list removing the head node and inserting it to the tail. */
void redis_list_rotate_head_to_tail(redis_dlist_t *rlist)
{
    /* Detach current head */
    redis_dlist_node_t *head = rlist->head;
    head->next->prev = NULL;
    rlist->head = head->next;

    /* Move it as tail */
    rlist->tail->next = head;
    head->next = NULL;
    head->prev = rlist->tail;
    rlist->tail = head;
}

/* Rotate the list removing the tail node and inserting it to the head. */
void redis_list_rotate_tail_to_head(redis_dlist_t *rlist)
{
    if (rlist->len <= 1) {
        return;
    }

    /* Detach current tail */
    redis_dlist_node_t *tail = rlist->tail;
    rlist->tail = tail->prev;
    tail->prev->next = NULL;

    /* Move it as head */
    rlist->head->prev = tail;
    tail->next = rlist->head;
    tail->prev = NULL;
    rlist->head = tail;
}

redis_dlist_t *redis_list_dup(redis_dlist_t *ori_rlist)
{
    redis_dlist_t *copy;
    redis_list_iter_t iter;
    redis_dlist_node_t *node;

    if ((copy = redis_list_create()) == NULL) {
        return NULL;
    }
    REDIS_LIST_SET_DUP_METHOD(copy, ori_rlist->dup);
    REDIS_LIST_SET_MATCH_METHOD(copy, ori_rlist->match);
    REDIS_LIST_SET_FREE_METHOD(copy, ori_rlist->free);

    redis_list_iter_rewind(ori_rlist, &iter);
    while ((node = redis_list_next(&iter)) != NULL) {
        void *value = NULL;
        if (copy->dup) {
            if ((value = copy->dup(node->val)) == NULL) {
                redis_list_release(copy);
                return NULL;
            }
        } else {
            value = node->val;
        }

        if (redis_list_add_node_tail(copy, value) == NULL) {
            /* Free value if dup succeed but listAddNodeTail failed. */
            if (copy->free) {
                copy->free(value);
                redis_list_release(copy);
                return NULL;
            }
        }
    }

    return NULL;
}

/* Search the list for a node matching a given key. */
redis_dlist_node_t *redis_list_search_key(redis_dlist_t *rlist, void *key)
{
    redis_list_iter_t iter;
    redis_dlist_node_t *node;

    redis_list_iter_rewind(rlist, &iter);
    while((node = redis_list_next(&iter)) != NULL) {
        if (rlist->match) {
            if (rlist->match(node->val, key)) {
                return node;
            }
        } else {
            if (key == node->val) {
                return node;
            }
        }
    }

    return NULL;
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned.
 */
redis_dlist_node_t *redis_list_index(redis_dlist_t *rlist, long index)
{
    redis_dlist_node_t *target_node;

    if (index < 0) {
        index = (-index) - 1;
        target_node = rlist->tail;
        while (index-- && target_node) {
            target_node = target_node->prev;
        }
    } else {
        target_node = rlist->head;
        while (index-- && target_node) {
            target_node = target_node->next;
        }
    }

    return target_node;
}

/* Add all the elements of the list 'o' at the end of the
 * list 'l'. The list 'other' remains empty but otherwise valid.
 */
redis_dlist_t *redis_list_join(redis_dlist_t *l, redis_dlist_t *o)
{
    if (o->len == 0) {
        return l;
    }

    if (l->tail) {
        l->tail->next = o->head;
    } else {
        l->head = o->head;
    }
    l->tail = o->tail;
    l->len += o->len;

    /* Setup other as an empty list. */
    o->head = o->tail = NULL;
    o->len = 0;

    return l;
}

/* Initializes the node's value and sets its pointers
 * so that it is initially not a member of any list.
 */
void redis_list_init_node(redis_dlist_node_t *node, void *value)
{
    node->val = value;
    node->next = node->prev = NULL;
}