#include "todo_plan.h"
#include "todo_model.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int plan_import_template(int template_id, int64_t start_date, int64_t task_system_id) {
    (void)template_id; (void)start_date; (void)task_system_id;
    return 0;
}

int plan_expand_to_todos(int64_t plan_id, int64_t task_system_id) {
    (void)plan_id; (void)task_system_id;
    return 0;
}

int plan_get_phase_progress(int64_t plan_id, double *progress, int *count) {
    (void)plan_id;
    *progress = 0.0;
    *count = 0;
    return 0;
}