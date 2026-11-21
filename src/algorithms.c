#include "fdtool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Calcula o fecho de atributos X+ sob o conjunto de DFs `fds` pelo metodo de ponto fixo.
 *
 * Parametros opcionais de filtragem (usados na verificacao de DFs redundantes):
 * - `removed`: vetor booleano indicando DFs ja descartadas (pode ser NULL).
 * - `skip_idx`: indice de uma DF especifica a ignorar nesta execucao
 *               (passe `fds->count` quando nao quiser ignorar nenhuma).
 */
static AttrMask compute_closure_skip(AttrMask x, const FDList *fds,
                                     const bool *removed, size_t skip_idx) {
    AttrMask closure = x; /* X sempre esta contido em X+ (reflexividade) */
    bool changed = true;

    /*
     * Repete passagens sobre F enquanto novos atributos forem descobertos.
     * Como |U| <= 26 e cada iteracao util adiciona ao menos 1 bit, o laco
     * executa no maximo 26 vezes.
     */
    while (changed) {
        changed = false;
        for (size_t i = 0; i < fds->count; i++) {
            /* Pula a DF sendo testada para redundancia ou DFs ja removidas */
            if (i == skip_idx || (removed && removed[i])) {
                continue;
            }

            AttrMask lhs = fds->items[i].lhs;
            /* Se LHS esta contido no fecho atual (LHS <= closure), aplica LHS -> RHS */
            if ((lhs & closure) == lhs) {
                AttrMask next = closure | fds->items[i].rhs;
                /* Marca alteracao apenas se algum atributo novo foi realmente adicionado */
                if (next != closure) {
                    closure = next;
                    changed = true;
                }
            }
        }
    }
    return closure;
}

/* Versao publica do calculo de fecho X+ considerando todas as DFs de `fds` */
AttrMask compute_closure(AttrMask x, const FDList *fds) {
    return compute_closure_skip(x, fds, NULL, fds->count);
}

/* Verifica se a DF exata (lhs -> rhs) ja existe na lista para evitar duplicatas */
static bool fdlist_contains(const FDList *list, AttrMask lhs, AttrMask rhs) {
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].lhs == lhs && list->items[i].rhs == rhs) {
            return true;
        }
    }
    return false;
}

/*
 * Aplica a regra da decomposicao de Armstrong: transforma cada X -> Y em
 * varias DFs unitarias X -> A (para cada atributo A em Y, em ordem A..Z).
 *
 * Tambem descarta:
 * - DFs triviais (quando A ja pertence a X, validas por reflexividade);
 * - DFs identicas repetidas, preservando a ordem original de aparicao.
 */
static FDList decompose_unitary(const FDList *fds) {
    FDList unitary;
    fdlist_init(&unitary);

    for (size_t i = 0; i < fds->count; i++) {
        AttrMask lhs = fds->items[i].lhs;
        AttrMask rhs = fds->items[i].rhs;

        /* Percorre os 26 bits em ordem crescente (A..Z) para manter RHS ordenado */
        for (int bit = 0; bit < MAX_ATTRS; bit++) {
            AttrMask attr = (AttrMask)(1u << bit);
            if ((rhs & attr) == 0u) {
                continue;
            }
            /* Ignora DF trivial: se A pertence ao LHS, X -> A nao traz informacao nova */
            if ((lhs & attr) != 0u) {
                continue;
            }
            if (!fdlist_contains(&unitary, lhs, attr)) {
                fdlist_push(&unitary, lhs, attr);
            }
        }
    }

    return unitary;
}
