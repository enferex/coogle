#include <iterator>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include "cs.h"


typedef std::unordered_multimap<std::string, const cs_sym_t *> htype;
struct db_t
{
    htype hash;
};


// Create a hashable object for each entry
#ifdef __cplusplus
extern "C"
#endif
cs_db_t build_database(const cs_t *cs)
{
    auto db = new(db_t);

    for (const cs_file_t *f=cs->files; f; f=f->next)
      for (const cs_sym_t *s=f->syms; s; s=s->next)
        db->hash.insert(std::make_pair(std::string(s->name), s));

    return static_cast<cs_db_t>(db);
}


// Create a list of matching symbols
// The result must be deallocated via free_search()
#ifdef __cplusplus
extern "C"
#endif
const cs_sym_t **search(const cs_db_t *db, const char *match, int *n_found)
{
    if (n_found)
      *n_found = 0;

    auto d = reinterpret_cast<const db_t*>(db);
    std::vector<const cs_sym_t *> matches;
    std::regex re(match);

    for (auto itr=d->hash.begin(); itr!=d->hash.end(); ++itr)
      if (std::regex_match((*itr).second->name, re))
        matches.push_back((*itr).second);
#if 0
    auto itrs = reinterpret_cast<const db_t*>(db)->hash.equal_range(match);
    auto itr = itrs.first;
    auto n = std::distance(itrs.first, itrs.second);
    for (int i=0; i<n; ++i)
      rslt[i] = (itr++)->second;
#endif
    auto rslt = new const cs_sym_t *[matches.size()];
    for (int i=0; i<matches.size(); ++i)
      rslt[i] = matches[i];
    *n_found = matches.size();

    return rslt;
}

#ifdef __cplusplus
extern "C"
#endif
void free_search(cs_sym_t *syms)
{
    delete syms;
}
