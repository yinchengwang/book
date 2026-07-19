#include "todo_stats.h"
#include "todo_model.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

int stats_calculate(dfx_stats_t *stats) {
    memset(stats, 0, sizeof(*stats));
    return 0;
}

int stats_calculate_heatmap(heatmap_data_t *heatmap) {
    memset(heatmap, 0, sizeof(*heatmap));
    return 0;
}