#define __USE_POSIX
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "cs.h"
#include "build.hh"


static void usage(const char *execname)
{
    printf("Usage: %s <cscope.out>\n", execname);
    exit(EXIT_SUCCESS);
}


typedef struct { size_t off; size_t data_len; const char *data; } pos_t;


#define VALID(_p)     ((_p)->off <= (_p)->data_len)
#define NXT_VALID(_p) ((_p)->off+1 <= (_p)->data_len)
#define END(_p)       ((_p)->off >= (_p)->data_len)
#define CH(_p)        (VALID(_p) ? (_p)->data[(_p)->off] : EOF)
#define ERR(...) do {fprintf(stderr,__VA_ARGS__);fputc('\n', stderr);} while(0)


#ifdef DEBUG
#define DBG(...) do {fprintf(stdout,__VA_ARGS__);fputc('\n', stdout);} while(0)
#else
#define DBG(...) 
#endif


static void get_line(pos_t *pos, char *buf, size_t buf_len)
{
    size_t len, st = pos->off;

    /* Get initial line (header line) */
    while (CH(pos) != EOF && CH(pos) != '\n')
      ++pos->off;

    len = pos->off - st; /* Don't return the '\n'     */
    ++pos->off;          /* +1 to advance to the '\n' */
    if (END(pos) || len > buf_len)
      return;

    memcpy(buf, pos->data + st, len);
    buf[len] = '\0';
}


/* Header looks like: 
 *     <cscope> <dir> <version> [-c] [-q <symbols>] [-T] <trailer>
 */
static void init_header(cs_t *cs, const char *data, size_t data_len)
{
    pos_t pos = {0};
    char buf[1024], *tok;
    
    pos.data = data;
    pos.data_len = data_len;
    get_line(&pos, buf, sizeof(buf));

    /* After the header are the symbols */
    cs->hdr.syms_start = pos.off;

    /* Load in the header: <cscope> */
    tok = strtok(buf, " ");
    if (strncmp(tok, "cscope", strlen("cscope")))
    {
        ERR("This does not appear to be a cscope database");
        return;
    }

    /* Version */
    cs->hdr.version = atoi(strtok(NULL, " "));
    
    /* Directory */
    cs->hdr.dir = strndup(strtok(NULL, " "), 1024);

    /* Optionals: [-c] [-T] [-q <syms>] */
    while ((tok = strtok(NULL, " ")))
    {
        if (tok[0] == '-' && strlen(tok) == 2)
        {
            if (tok[1] == 'c')
              cs->hdr.compression = true;
            else if (tok[1] == 'T')
              cs->hdr.prefix_match = true;
            else if (tok[1] == 'q')
              cs->hdr.inverted_index = true; // TODO
            else 
            {
                ERR("Unrecognized header option");
                return;
            }
        }
        else
        {
            cs->hdr.trailer = atol(tok);
            break;
        }
    }
}


static void init_trailer(cs_t *cs, const void *data, size_t data_len)
{
    int i;
    size_t n_bytes;
    char line[1024] = {0};
    pos_t pos = {0};

    pos.data = data;
    pos.data_len = data_len;
    pos.off = cs->hdr.trailer;

    if (!VALID(&pos))
      return;

    /* Viewpaths */
    get_line(&pos, line, sizeof(line));
    cs->trailer.n_viewpaths = atoi(line);
    for (i=0; i<cs->trailer.n_viewpaths; ++i)
    {
        get_line(&pos, line, sizeof(line));
        DBG("[%d of %d] Viewpath: %s", i+1, cs->trailer.n_viewpaths, line);
    }
    
    /* Sources */
    get_line(&pos, line, sizeof(line));
    cs->trailer.n_srcs = atoi(line);
    for (i=0; i<cs->trailer.n_srcs; ++i)
    {
        get_line(&pos, line, sizeof(line));
        DBG("[%d of %d] Sources: %s", i+1, cs->trailer.n_srcs, line);
    }
        
    /* Includes */
    get_line(&pos, line, sizeof(line));
    cs->trailer.n_incs = atoi(line);
    get_line(&pos, line, sizeof(line));
    n_bytes = atol(line);
    for (i=0; i<cs->trailer.n_incs; ++i)
    {
        get_line(&pos, line, sizeof(line));
        DBG("[%d of %d] Includes: %s", i+1, cs->trailer.n_incs, line);
    }
}


static cs_file_t *new_file(const char *str)
{
    cs_file_t *file;
    const char fname, mark, *c = str;
    
    /* Skip whitespace */
    while (isspace(*c))
      ++c;

    if (!(file = calloc(1, sizeof(cs_file_t))))
      ERR("Out of memory");
    file->mark = *c;
    file->name = strdup(c+1);
    return file;
}


static cs_sym_t *new_sym(void)
{
    cs_sym_t *sym;

    if (!(sym = calloc(1, sizeof(cs_file_t))))
      ERR("Out of memory");

    return sym;
}

static const char marks[] = 
{
    '@', '$', '`',
    '}', '#', ')',
    '~', '=', ';', 
    'c', 'e', 'g', 
    'l', 'm', 'p', 
    's', 't', 'u'
};


static _Bool is_mark(char c)
{
    int i;
    for (i=0; i<sizeof(marks)/sizeof(marks[0]); ++i)
      if (c == marks[i])
        return true;
    return false;
}


/* Extract the symbols for file
 * This must start with the <mark><file> line.
 */
static void file_load_symbols(cs_file_t *file, pos_t *pos)
{
    size_t lineno;
    char line[1024], *c; 
    cs_sym_t *sym, *prev;

    DBG("Loading: %s", file->name);
   
    /* <blank line> */
    get_line(pos, line, sizeof(line));

    while (VALID(pos))
    {
        /* Either this is a symbol, or a file.  If this is a file, then we are
         * done processing the current file.  So, in that case we break and
         * restore the position at the start of the next file's data:
         * <mark><file> line.
         *
         * So there are two cases here:
         * 1) New set of symbols: <lineno><blank><non-symbol text> 
         * 2) A new file: <mark><file>
         */
        get_line(pos, line, sizeof(line));
        c = line;
        while (isspace(*c))
          ++c;

        /* Case 2: New file */
        if (c[0] == '@')
        {
            pos->off -= strlen(line);
            return;
        }

        /* Case 1: Symbols! */
        lineno = atol(line);

        /* Suck in all the symbols for this lineno */
        while (VALID(pos)) 
        {
            /* <opt. mark><symbol> (or blank if end of symbol data) */
            get_line(pos, line, sizeof(line));
            if (strlen(line) == 0)
              break;

            c = line;

            /* Skip whitespace */
            while (isspace(*c))
              ++c;

            /* Skip lines only containing mark characters */
            if (is_mark(c[0]) && strlen(c+1) == 0)
              continue;

            sym = new_sym();
            if (is_mark(c[0]))
            {
                sym->mark = c[0];
                ++c;
            }

            if (strlen(c) == 0)
            {
                free(sym);
                continue;
            }

            sym->line = lineno;
            sym->name = strdup(c);
            sym->file = file;

            /* <non-symbol text> */
            get_line(pos, line, sizeof(line));

            /* Add */
            if (!file->syms)
              file->syms = sym;
            else
              prev->next = sym;
            prev = sym;
            ++file->n_syms;
        }
    }
}


static void init_symbols(cs_t *cs, const char *data, size_t data_len)
{
    pos_t pos = {0};
    char line[1024], *c; 
    cs_file_t *file, *prev;
   
    pos.off = cs->hdr.syms_start; 
    pos.data = data;
    pos.data_len = data_len;

    while (VALID(&pos) && pos.off <= cs->hdr.trailer)
    {
        /* Get file info */
        get_line(&pos, line, sizeof(line));
        file = new_file(line);
        file_load_symbols(file, &pos);
        if (!cs->files)
          cs->files = file;
        else
          prev->next = file;

        prev = file;
        ++cs->n_files;
        cs->n_syms += file->n_syms;
    }
}


/* Load a cscope database and return a pointer to the data */
static cs_t *load_cscope(const char *fname)
{
    FILE *fp;
    cs_t *cs;
    void *data;
    struct stat st;

    if (!(fp = fopen(fname, "r")))
    {
        fprintf(stderr, "Could not open cscope database file: %s\n", fname);
        exit(EXIT_FAILURE);
    }

    /* mmap the input cscope database */
    fstat(fileno(fp), &st);
    data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fileno(fp), 0);
    if (!data)
    {
        fprintf(stderr, "Error maping cscope database\n");
        exit(EXIT_FAILURE);
    }

    if (!(cs = calloc(sizeof(cs_t), 1)))
    {
        ERR("Could not allocate enough room for cscope parent structure");
        return NULL;
    }

    cs->name = strdup(fname);
    init_header(cs, data, st.st_size);
    init_trailer(cs, data, st.st_size);
    init_symbols(cs, data, st.st_size);
    
    munmap(data, st.st_size);
    fclose(fp);
    return cs;
}


#ifdef DEBUG
static void cs_dump(FILE *fp, const cs_t *cs)
{
    int i;
    const cs_sym_t  *s;
    const cs_file_t *f;

    fprintf(fp, "%s: cscope database dump\n", cs->name);
    fprintf(fp, "files: %d\n", cs->n_files);
    for (f=cs->files; f; f=f->next)
    {
        printf("\t%s: %d symbols\n", f->name, f->n_syms);
        i = 0;
        for (s=f->syms; s; s=s->next)
          fprintf(fp, "\t\t[%d of %d] %s: %s\n", ++i, f->n_syms, f->name, s->name);
    }
}
#endif /* DEBUG */


int main(int argc, char **argv)
{
    int i, n_matches;
    char *user;
    const char *fname;
    const cs_sym_t **matches, *sym;
    cs_t *cs;
    cs_db_t *db;

    if (argc != 2)
      usage(argv[0]);

    fname = argv[1];

    /* Load */
    cs = load_cscope(fname);
    db = build_database(cs);

#ifdef DEBUG
    cs_dump(stdout, cs);
#endif

    /* Summary */
    printf("Loaded %d symbols from %d files\n", cs->n_syms, cs->n_files);

    while ((user = readline("Search> ")))
    {
        if (user && *user)
          add_history(user);

        matches = search(db, user, &n_matches);
        printf("Located %d matches:\n", n_matches);
        for (i=0; i<n_matches; ++i)
        {
            sym = matches[i];
            printf("%d) %s in %s:%zu\n", i+1, sym->name, sym->file->name, sym->line);
        }
        free_search(matches);
    }

    return 0;
}
