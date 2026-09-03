#include <algo-prod/queue/queue.h>

#include <stdlib.h>
#include <string.h>

#define QUEUE_DEFAULT_CAPACITY 16u
#define QUEUE_GROWTH_FACTOR 2u

struct queue {
    void *data;
    size_t size;
    size_t capacity;
    size_t head;
    size_t element_size;
    void (*free_element)(void *element);
};

static unsigned char *queue_slot(queue_t *queue, size_t index)
{
    return (unsigned char *)queue->data + index * queue->element_size;
}

static const unsigned char *queue_slot_const(const queue_t *queue, size_t index)
{
    return (const unsigned char *)queue->data + index * queue->element_size;
}

static size_t queue_physical_index(const queue_t *queue, size_t logical_index)
{
    return (queue->head + logical_index) % queue->capacity;
}

static int queue_set_capacity(queue_t *queue, size_t new_capacity)
{
    void *new_data;
    size_t index;

    if (!queue || new_capacity < queue->size || new_capacity == 0u) {
        return -1;
    }

    new_data = malloc(new_capacity * queue->element_size);
    if (!new_data) {
        return -1;
    }

    for (index = 0u; index < queue->size; ++index) {
        memcpy((unsigned char *)new_data + index * queue->element_size,
               queue_slot_const(queue, queue_physical_index(queue, index)),
               queue->element_size);
    }

    free(queue->data);
    queue->data = new_data;
    queue->capacity = new_capacity;
    queue->head = 0u;
    return 0;
}

static int queue_grow(queue_t *queue)
{
    size_t new_capacity;

    new_capacity = queue->capacity * QUEUE_GROWTH_FACTOR;
    if (new_capacity == queue->capacity) {
        new_capacity = queue->capacity + 1u;
    }

    return queue_set_capacity(queue, new_capacity);
}

queue_t *queue_create(size_t element_size)
{
    return queue_create_ex(element_size, 0u, NULL);
}

queue_t *queue_create_ex(size_t element_size,
                         size_t initial_capacity,
                         void (*free_element)(void *element))
{
    queue_t *queue;

    if (element_size == 0u) {
        return NULL;
    }
    if (initial_capacity == 0u) {
        initial_capacity = QUEUE_DEFAULT_CAPACITY;
    }

    queue = (queue_t *)calloc(1, sizeof(queue_t));
    if (!queue) {
        return NULL;
    }

    queue->data = malloc(initial_capacity * element_size);
    if (!queue->data) {
        free(queue);
        return NULL;
    }

    queue->capacity = initial_capacity;
    queue->element_size = element_size;
    queue->free_element = free_element;
    return queue;
}

void queue_destroy(queue_t *queue)
{
    if (!queue) {
        return;
    }

    queue_clear(queue);
    free(queue->data);
    free(queue);
}

int queue_push(queue_t *queue, const void *element)
{
    size_t tail_index;

    if (!queue || !element) {
        return -1;
    }
    if (queue->size >= queue->capacity && queue_grow(queue) != 0) {
        return -1;
    }

    tail_index = queue_physical_index(queue, queue->size);
    memcpy(queue_slot(queue, tail_index), element, queue->element_size);
    queue->size += 1u;
    return 0;
}

int queue_push_batch(queue_t *queue, const void *elements, size_t count)
{
    const unsigned char *source;
    size_t required_capacity;
    size_t first_chunk;
    size_t tail_index;

    if (!queue) {
        return -1;
    }
    if (count == 0u) {
        return 0;
    }
    if (!elements) {
        return -1;
    }

    required_capacity = queue->size + count;
    if (required_capacity > queue->capacity) {
        size_t new_capacity = queue->capacity;

        while (new_capacity < required_capacity) {
            size_t next_capacity = new_capacity * QUEUE_GROWTH_FACTOR;
            if (next_capacity == new_capacity) {
                next_capacity = new_capacity + 1u;
            }
            new_capacity = next_capacity;
        }

        if (queue_set_capacity(queue, new_capacity) != 0) {
            return -1;
        }
    }

    source = (const unsigned char *)elements;
    tail_index = queue_physical_index(queue, queue->size);
    first_chunk = queue->capacity - tail_index;
    if (first_chunk > count) {
        first_chunk = count;
    }

    memcpy(queue_slot(queue, tail_index),
           source,
           first_chunk * queue->element_size);

    if (first_chunk < count) {
        memcpy(queue_slot(queue, 0u),
               source + first_chunk * queue->element_size,
               (count - first_chunk) * queue->element_size);
    }

    queue->size += count;
    return 0;
}

int queue_pop(queue_t *queue, void *out)
{
    unsigned char *front;

    if (!queue || queue->size == 0u) {
        return -1;
    }

    front = queue_slot(queue, queue->head);
    if (out) {
        memcpy(out, front, queue->element_size);
    } else if (queue->free_element) {
        queue->free_element(front);
    }

    queue->head = (queue->head + 1u) % queue->capacity;
    queue->size -= 1u;
    if (queue->size == 0u) {
        queue->head = 0u;
    }
    return 0;
}

int queue_front(const queue_t *queue, void *out)
{
    if (!queue || queue->size == 0u || !out) {
        return -1;
    }

    memcpy(out, queue_slot_const(queue, queue->head), queue->element_size);
    return 0;
}

int queue_back(const queue_t *queue, void *out)
{
    size_t tail_index;

    if (!queue || queue->size == 0u || !out) {
        return -1;
    }

    tail_index = queue_physical_index(queue, queue->size - 1u);
    memcpy(out, queue_slot_const(queue, tail_index), queue->element_size);
    return 0;
}

const void *queue_front_ptr(const queue_t *queue)
{
    if (!queue || queue->size == 0u) {
        return NULL;
    }

    return queue_slot_const(queue, queue->head);
}

const void *queue_back_ptr(const queue_t *queue)
{
    size_t tail_index;

    if (!queue || queue->size == 0u) {
        return NULL;
    }

    tail_index = queue_physical_index(queue, queue->size - 1u);
    return queue_slot_const(queue, tail_index);
}

size_t queue_size(const queue_t *queue)
{
    return queue ? queue->size : 0u;
}

size_t queue_capacity(const queue_t *queue)
{
    return queue ? queue->capacity : 0u;
}

bool queue_empty(const queue_t *queue)
{
    return !queue || queue->size == 0u;
}

void queue_clear(queue_t *queue)
{
    if (!queue) {
        return;
    }

    if (queue->free_element) {
        while (queue->size > 0u) {
            queue->free_element(queue_slot(queue, queue->head));
            queue->head = (queue->head + 1u) % queue->capacity;
            queue->size -= 1u;
        }
    } else {
        queue->size = 0u;
    }
    queue->head = 0u;
}