#include <algo-prod/list/list.h>

#include <stdlib.h>
#include <string.h>

struct list_node {
    struct list_node *prev;
    struct list_node *next;
    unsigned char data[];
};

struct list {
    size_t element_size;
    size_t size;
    list_node_t *head;
    list_node_t *tail;
    void (*free_element)(void *element);
};

static list_node_t *list_node_create(size_t element_size, const void *element)
{
    list_node_t *node;

    node = (list_node_t *)malloc(sizeof(list_node_t) + element_size);
    if (!node) {
        return NULL;
    }

    node->prev = NULL;
    node->next = NULL;
    memcpy(node->data, element, element_size);
    return node;
}

static void list_node_destroy(list_t *list, list_node_t *node, int transfer_ownership, void *out)
{
    if (!list || !node) {
        return;
    }

    if (out) {
        memcpy(out, node->data, list->element_size);
    } else if (!transfer_ownership && list->free_element) {
        list->free_element(node->data);
    }

    free(node);
}

list_t *list_create(size_t element_size)
{
    return list_create_ex(element_size, NULL);
}

list_t *list_create_ex(size_t element_size, void (*free_element)(void *element))
{
    list_t *list;

    if (element_size == 0u) {
        return NULL;
    }

    list = (list_t *)calloc(1, sizeof(list_t));
    if (!list) {
        return NULL;
    }

    list->element_size = element_size;
    list->free_element = free_element;
    return list;
}

void list_destroy(list_t *list)
{
    if (!list) {
        return;
    }

    list_clear(list);
    free(list);
}

int list_push_front(list_t *list, const void *element)
{
    list_node_t *node;

    if (!list || !element) {
        return -1;
    }

    node = list_node_create(list->element_size, element);
    if (!node) {
        return -1;
    }

    node->next = list->head;
    if (list->head) {
        list->head->prev = node;
    } else {
        list->tail = node;
    }
    list->head = node;
    list->size += 1u;
    return 0;
}

int list_push_back(list_t *list, const void *element)
{
    list_node_t *node;

    if (!list || !element) {
        return -1;
    }

    node = list_node_create(list->element_size, element);
    if (!node) {
        return -1;
    }

    node->prev = list->tail;
    if (list->tail) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    list->tail = node;
    list->size += 1u;
    return 0;
}

int list_pop_front(list_t *list, void *out)
{
    list_node_t *node;

    if (!list || !list->head) {
        return -1;
    }

    node = list->head;
    list->head = node->next;
    if (list->head) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL;
    }
    list->size -= 1u;
    list_node_destroy(list, node, out != NULL, out);
    return 0;
}

int list_pop_back(list_t *list, void *out)
{
    list_node_t *node;

    if (!list || !list->tail) {
        return -1;
    }

    node = list->tail;
    list->tail = node->prev;
    if (list->tail) {
        list->tail->next = NULL;
    } else {
        list->head = NULL;
    }
    list->size -= 1u;
    list_node_destroy(list, node, out != NULL, out);
    return 0;
}

const void *list_front_ptr(const list_t *list)
{
    return (list && list->head) ? list->head->data : NULL;
}

const void *list_back_ptr(const list_t *list)
{
    return (list && list->tail) ? list->tail->data : NULL;
}

const list_node_t *list_front_node(const list_t *list)
{
    return list ? list->head : NULL;
}

const list_node_t *list_back_node(const list_t *list)
{
    return list ? list->tail : NULL;
}

const list_node_t *list_node_next(const list_node_t *node)
{
    return node ? node->next : NULL;
}

const list_node_t *list_node_prev(const list_node_t *node)
{
    return node ? node->prev : NULL;
}

const void *list_node_value(const list_node_t *node)
{
    return node ? node->data : NULL;
}

size_t list_size(const list_t *list)
{
    return list ? list->size : 0u;
}

bool list_empty(const list_t *list)
{
    return !list || list->size == 0u;
}

void list_clear(list_t *list)
{
    list_node_t *node;
    list_node_t *next;

    if (!list) {
        return;
    }

    node = list->head;
    while (node) {
        next = node->next;
        list_node_destroy(list, node, 0, NULL);
        node = next;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0u;
}