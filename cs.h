#ifndef _CS_H
#define _CS_H
#include <stdbool.h>


/* Opaque handle */
typedef void *cs_db_t;


/* Symbol */
struct _cscope_file_t;
typedef struct _cscope_sym_t
{
    char                         mark;
    size_t                       line;
    const char                  *name;
    const struct _cscope_file_t *file;
    struct _cscope_sym_t        *next;
} cs_sym_t;


/* A file entry contains a list of symbols */
typedef struct _cscope_file_t
{
    char                   mark;
    const char            *name;
    int                    n_syms;
    cs_sym_t              *syms;
    struct _cscope_file_t *next;
} cs_file_t;


/* cscope database (cscope.out) header */
typedef struct _cscope_hdr_t
{
    int         version;
    _Bool       compression;    /* -c */
    _Bool       inverted_index; /* -q */
    _Bool       prefix_match;   /* -T */
    size_t      syms_start;
    size_t      trailer;
    const char *dir;
} cs_hdr_t;


/* cscope database (cscope.out) trailer */
typedef struct _cscope_trl_t
{
    int    n_viewpaths;
    char **viewpath_dirs;
    int    n_srcs;
    char **srcs;
    int    n_incs;
    char **incs;
} cs_trl_t;


/* cscope database, contains a list of file entries */
typedef struct _cscope_t
{
    cs_hdr_t    hdr;
    cs_trl_t    trailer;
    const char *name;
    int         n_files;
    int         n_syms;
    cs_file_t  *files;
} cs_t;

#endif /* _CS_H */
