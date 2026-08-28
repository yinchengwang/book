#ifndef NOTES_API_MOD_H
#define NOTES_API_MOD_H

#include "../../common/http_router.h"

void register_notes_routes(Router *r, const char *notes_root);

#endif /* NOTES_API_MOD_H */
