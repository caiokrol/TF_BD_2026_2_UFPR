#ifndef FDTOOL_H
#define FDTOOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_ATTRS 26

/* Representa um conjunto de atributos (A..Z) como mascara de bits */
typedef uint32_t AttrMask;

/* Dependencia funcional: lhs -> rhs */
typedef struct {
    AttrMask lhs;
    AttrMask rhs;
} FD;

/* Lista dinamica de dependencias funcionais */
typedef struct {
    FD *items;
    size_t count;
    size_t capacity;
} FDList;

/* Lista dinamica de mascaras de atributos (ex: chaves candidatas) */
typedef struct {
    AttrMask *items;
    size_t count;
    size_t capacity;
} MaskList;

/* Esquema relacional lido de um arquivo .fds */
typedef struct {
    AttrMask universe;
    FDList fds;
} Schema;

/* Utilitarios de mascara de atributos */
AttrMask attr_bit(char c);
int mask_size(AttrMask mask);
void mask_to_str(AttrMask mask, char *buf);
bool parse_attr_string(const char *str, AttrMask universe, AttrMask *out);

/* Gerenciamento de listas dinamicas */
void fdlist_init(FDList *list);
void fdlist_push(FDList *list, AttrMask lhs, AttrMask rhs);
void fdlist_free(FDList *list);

void masklist_init(MaskList *list);
void masklist_push(MaskList *list, AttrMask mask);
void masklist_free(MaskList *list);

/* Parser do arquivo .fds */
bool parse_fds_file(const char *filepath, Schema *schema);
void schema_free(Schema *schema);

/* Algoritmos principais */
AttrMask compute_closure(AttrMask x, const FDList *fds);
FDList compute_mincover(const FDList *fds);
MaskList compute_candidate_keys(AttrMask universe, const FDList *fds);
void check_normal_forms(AttrMask universe, const FDList *fds);

#endif /* FDTOOL_H */