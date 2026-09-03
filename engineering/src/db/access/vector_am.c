#include "db/access/vector_am.h"

void vector_am_init(void)
{
    index_am_register("hnsw", &hnsw_am_routine);
    index_am_register("ivf", &ivf_am_routine);
}
