/**
 * @file conflict_resolution.c
 * @brief Conflict Resolution - Vector Clock and CRDT Implementation
 */

#include "db/consistency/conflict_resolution.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Vector Clock Implementation
 * ============================================================ */

void vc_init(vector_clock_t *vc, int local_node_id, int node_count) {
    if (!vc || node_count <= 0 || node_count > MAX_NODES) {
        return;
    }
    memset(vc->clocks, 0, sizeof(vc->clocks));
    vc->node_count = node_count;
    vc->local_node_id = local_node_id;
}

void vc_inc(vector_clock_t *vc) {
    if (!vc || vc->local_node_id < 0 || vc->local_node_id >= vc->node_count) {
        return;
    }
    vc->clocks[vc->local_node_id]++;
}

void vc_merge(vector_clock_t *vc, const vector_clock_t *other) {
    if (!vc || !other) {
        return;
    }
    int max_nodes = (vc->node_count < other->node_count) ?
                    vc->node_count : other->node_count;
    for (int i = 0; i < max_nodes; i++) {
        if (other->clocks[i] > vc->clocks[i]) {
            vc->clocks[i] = other->clocks[i];
        }
    }
}

int vc_compare(const vector_clock_t *a, const vector_clock_t *b) {
    if (!a || !b) {
        return 0;
    }
    bool a_greater = false;
    bool b_greater = false;
    int min_count = (a->node_count < b->node_count) ?
                    a->node_count : b->node_count;

    for (int i = 0; i < min_count; i++) {
        if (a->clocks[i] > b->clocks[i]) {
            a_greater = true;
        } else if (a->clocks[i] < b->clocks[i]) {
            b_greater = true;
        }
    }
    /* Check remaining elements if node counts differ */
    if (a->node_count > min_count) {
        for (int i = min_count; i < a->node_count; i++) {
            if (a->clocks[i] > 0) {
                a_greater = true;
            }
        }
    }
    if (b->node_count > min_count) {
        for (int i = min_count; i < b->node_count; i++) {
            if (b->clocks[i] > 0) {
                b_greater = true;
            }
        }
    }

    if (a_greater && !b_greater) return 1;   /* a happened after b */
    if (b_greater && !a_greater) return -1;  /* a happened before b */
    return 0;                                 /* concurrent */
}

bool vc_is_concurrent(const vector_clock_t *a, const vector_clock_t *b) {
    return vc_compare(a, b) == 0;
}

void vc_copy(vector_clock_t *dest, const vector_clock_t *src) {
    if (!dest || !src) {
        return;
    }
    memcpy(dest, src, sizeof(vector_clock_t));
}

/* ============================================================
 * CRDT Implementation
 * ============================================================ */

struct crdt {
    crdt_type_t type;
    union {
        struct {
            uint64_t *nodes;     /**< Per-node counters for G-Counter */
            int node_count;
            int local_node_id;
        } gcounter;
        struct {
            uint64_t *nodes_p;   /**< Per-node positive counters */
            uint64_t *nodes_n;   /**< Per-node negative counters */
            int node_count;
            int local_node_id;
        } pncounter;
        struct {
            uint64_t value;      /**< Current value */
            uint64_t timestamp;  /**< Timestamp of last write */
        } lwwreg;
    } data;
};

/**
 * @brief Check if CRDT type is valid
 */
static bool crdt_type_valid(crdt_type_t type) {
    return type == CRDT_GCOUNTER || type == CRDT_PNCOUNTER || type == CRDT_LWWREG;
}

crdt_t *crdt_create(crdt_type_t type) {
    if (!crdt_type_valid(type)) {
        return NULL;
    }

    crdt_t *crdt = (crdt_t *)calloc(1, sizeof(crdt_t));
    if (!crdt) {
        return NULL;
    }

    crdt->type = type;

    switch (type) {
        case CRDT_GCOUNTER:
            crdt->data.gcounter.node_count = MAX_NODES;
            crdt->data.gcounter.local_node_id = 0;
            crdt->data.gcounter.nodes = (uint64_t *)calloc(MAX_NODES, sizeof(uint64_t));
            if (!crdt->data.gcounter.nodes) {
                free(crdt);
                return NULL;
            }
            break;

        case CRDT_PNCOUNTER:
            crdt->data.pncounter.node_count = MAX_NODES;
            crdt->data.pncounter.local_node_id = 0;
            crdt->data.pncounter.nodes_p = (uint64_t *)calloc(MAX_NODES, sizeof(uint64_t));
            crdt->data.pncounter.nodes_n = (uint64_t *)calloc(MAX_NODES, sizeof(uint64_t));
            if (!crdt->data.pncounter.nodes_p || !crdt->data.pncounter.nodes_n) {
                free(crdt->data.pncounter.nodes_p);
                free(crdt->data.pncounter.nodes_n);
                free(crdt);
                return NULL;
            }
            break;

        case CRDT_LWWREG:
            crdt->data.lwwreg.value = 0;
            crdt->data.lwwreg.timestamp = 0;
            break;
    }

    return crdt;
}

void crdt_destroy(crdt_t *crdt) {
    if (!crdt) {
        return;
    }

    switch (crdt->type) {
        case CRDT_GCOUNTER:
            free(crdt->data.gcounter.nodes);
            break;
        case CRDT_PNCOUNTER:
            free(crdt->data.pncounter.nodes_p);
            free(crdt->data.pncounter.nodes_n);
            break;
        case CRDT_LWWREG:
            /* Nothing to free */
            break;
    }

    free(crdt);
}

int crdt_merge(crdt_t *a, const crdt_t *b) {
    if (!a || !b || a->type != b->type) {
        return -1;
    }

    switch (a->type) {
        case CRDT_GCOUNTER: {
            for (int i = 0; i < MAX_NODES; i++) {
                if (b->data.gcounter.nodes[i] > a->data.gcounter.nodes[i]) {
                    a->data.gcounter.nodes[i] = b->data.gcounter.nodes[i];
                }
            }
            break;
        }
        case CRDT_PNCOUNTER: {
            for (int i = 0; i < MAX_NODES; i++) {
                if (b->data.pncounter.nodes_p[i] > a->data.pncounter.nodes_p[i]) {
                    a->data.pncounter.nodes_p[i] = b->data.pncounter.nodes_p[i];
                }
                if (b->data.pncounter.nodes_n[i] > a->data.pncounter.nodes_n[i]) {
                    a->data.pncounter.nodes_n[i] = b->data.pncounter.nodes_n[i];
                }
            }
            break;
        }
        case CRDT_LWWREG: {
            if (b->data.lwwreg.timestamp > a->data.lwwreg.timestamp) {
                a->data.lwwreg.value = b->data.lwwreg.value;
                a->data.lwwreg.timestamp = b->data.lwwreg.timestamp;
            }
            break;
        }
    }

    return 0;
}

int64_t crdt_get_value(const crdt_t *crdt) {
    if (!crdt) {
        return 0;
    }

    switch (crdt->type) {
        case CRDT_GCOUNTER: {
            uint64_t sum = 0;
            for (int i = 0; i < MAX_NODES; i++) {
                sum += crdt->data.gcounter.nodes[i];
            }
            return (int64_t)sum;
        }
        case CRDT_PNCOUNTER: {
            int64_t sum_p = 0, sum_n = 0;
            for (int i = 0; i < MAX_NODES; i++) {
                sum_p += crdt->data.pncounter.nodes_p[i];
                sum_n += crdt->data.pncounter.nodes_n[i];
            }
            return (int64_t)(sum_p - sum_n);
        }
        case CRDT_LWWREG:
            return (int64_t)crdt->data.lwwreg.value;
    }

    return 0;
}

int crdt_inc(crdt_t *crdt, uint64_t delta) {
    if (!crdt || delta == 0) {
        return -1;
    }

    switch (crdt->type) {
        case CRDT_GCOUNTER:
            crdt->data.gcounter.nodes[crdt->data.gcounter.local_node_id] += delta;
            return 0;

        case CRDT_PNCOUNTER:
            crdt->data.pncounter.nodes_p[crdt->data.pncounter.local_node_id] += delta;
            return 0;

        case CRDT_LWWREG:
            /* LWW-Register uses crdt_set, not inc */
            return -1;
    }

    return -1;
}

int crdt_dec(crdt_t *crdt, uint64_t delta) {
    if (!crdt || delta == 0) {
        return -1;
    }

    switch (crdt->type) {
        case CRDT_GCOUNTER:
            /* G-Counter cannot decrement */
            return -1;

        case CRDT_PNCOUNTER:
            crdt->data.pncounter.nodes_n[crdt->data.pncounter.local_node_id] += delta;
            return 0;

        case CRDT_LWWREG:
            /* LWW-Register uses crdt_set, not dec */
            return -1;
    }

    return -1;
}

int crdt_set(crdt_t *crdt, uint64_t value, uint64_t timestamp) {
    if (!crdt || crdt->type != CRDT_LWWREG) {
        return -1;
    }

    if (timestamp > crdt->data.lwwreg.timestamp) {
        crdt->data.lwwreg.value = value;
        crdt->data.lwwreg.timestamp = timestamp;
    }

    return 0;
}

crdt_type_t crdt_get_type(const crdt_t *crdt) {
    if (!crdt) {
        return CRDT_GCOUNTER; /* Default */
    }
    return crdt->type;
}
