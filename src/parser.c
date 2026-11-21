#include "fdtool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Converte uma letra A..Z em sua mascara de 1 bit; retorna 0 para demais caracteres */
AttrMask attr_bit(char c) {
    return (c >= 'A' && c <= 'Z') ? (AttrMask)(1u << (c - 'A')) : 0u;
}

/* Conta quantos atributos (bits em 1) existem na mascara */
int mask_size(AttrMask mask) {
    int count = 0;
    for (int i = 0; i < MAX_ATTRS; i++) {
        if (mask & (1u << i)) count++;
    }
    return count;
}

/* Converte a mascara de volta para string em ordem alfabetica (A..Z) */
void mask_to_str(AttrMask mask, char *buf) {
    int pos = 0;
    for (int i = 0; i < MAX_ATTRS; i++) {
        if (mask & (1u << i)) buf[pos++] = (char)('A' + i);
    }
    buf[pos] = '\0';
}

/* Converte o argumento --X (ex: "BD") em mascara de bits */
bool parse_attr_string(const char *str, AttrMask universe, AttrMask *out) {
    (void)universe; /* entrada assumida valida */
    AttrMask mask = 0u;
    for (const char *p = str; *p != '\0'; p++) {
        mask |= attr_bit(*p);
    }
    *out = mask;
    return true;
}

/* Inicializa lista dinamica de DFs */
void fdlist_init(FDList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* Insere uma DF no final da lista, dobrando a capacidade quando necessario */
void fdlist_push(FDList *list, AttrMask lhs, AttrMask rhs) {
    if (list->count == list->capacity) {
        list->capacity = (list->capacity == 0) ? 16 : list->capacity * 2;
        list->items = (FD *)realloc(list->items, list->capacity * sizeof(FD));
    }
    list->items[list->count++] = (FD){lhs, rhs};
}

/* Libera a memoria alocada da lista de DFs */
void fdlist_free(FDList *list) {
    free(list->items);
    fdlist_init(list);
}

/* Inicializa lista dinamica de mascaras (ex: chaves candidatas) */
void masklist_init(MaskList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* Insere uma mascara no final da lista, dobrando a capacidade quando necessario */
void masklist_push(MaskList *list, AttrMask mask) {
    if (list->count == list->capacity) {
        list->capacity = (list->capacity == 0) ? 16 : list->capacity * 2;
        list->items = (AttrMask *)realloc(list->items, list->capacity * sizeof(AttrMask));
    }
    list->items[list->count++] = mask;
}

/* Libera a memoria alocada da lista de mascaras */
void masklist_free(MaskList *list) {
    free(list->items);
    masklist_init(list);
}

/* Libera todos os dados do esquema */
void schema_free(Schema *schema) {
    schema->universe = 0u;
    fdlist_free(&schema->fds);
}

/* Le o arquivo .fds (presumindo formato valido) e preenche U e F no esquema */
bool parse_fds_file(const char *filepath, Schema *schema) {
    schema->universe = 0u;
    fdlist_init(&schema->fds);

    /* Abre o arquivo e carrega todo o conteudo em memoria */
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *buf = (char *)malloc((size_t)size + 1);
    size_t n = fread(buf, 1, (size_t)size, fp);
    buf[n] = '\0';
    fclose(fp);

    /* Percorre o texto procurando os blocos U = {...} e F = {...} */
    const char *p = buf;
    while (*p != '\0') {
        if (*p == 'U' || *p == 'F') {
            char section = *p;
            p = strchr(p, '{') + 1;       /* inicio do conteudo dentro das chaves */
            const char *end = strchr(p, '}');

            if (section == 'U') {
                /* Liga o bit de cada atributo A..Z encontrado em U */
                for (const char *q = p; q < end; q++) {
                    schema->universe |= attr_bit(*q);
                }
            } else {
                /* Le cada DF separada por ',' ou ';' e dividida pela seta '->' */
                AttrMask lhs = 0u, rhs = 0u;
                bool in_rhs = false;
                for (const char *q = p; q <= end; q++) {
                    if (q == end || *q == ',' || *q == ';') {
                        /* Fim de uma DF: salva se nao for segmento vazio */
                        if (lhs != 0u && rhs != 0u) fdlist_push(&schema->fds, lhs, rhs);
                        lhs = rhs = 0u;
                        in_rhs = false;
                    } else if (*q == '-' && q + 1 < end && q[1] == '>') {
                        /* Passa do lado esquerdo (LHS) para o lado direito (RHS) */
                        in_rhs = true;
                        q++;
                    } else if (in_rhs) {
                        rhs |= attr_bit(*q);
                    } else {
                        lhs |= attr_bit(*q);
                    }
                }
            }
            p = end + 1; /* avanca para depois do '}' evitando reler letras do bloco */
        } else {
            p++;
        }
    }

    free(buf);
    return true;
}
