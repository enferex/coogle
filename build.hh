#ifndef _BUILD_HH
#define _BUILD_HH
#include "cs.h"

extern cs_db_t *build_database(const cs_t *cs);
extern const cs_sym_t **search(const cs_db_t *db, const char *match, int *n_found);
extern void free_search(const cs_sym_t **syms);

#endif // _BUILD_HH
