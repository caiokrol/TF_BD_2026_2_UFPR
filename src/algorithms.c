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

FDList compute_mincover(const FDList *fds) {
    /* Passo 1: decompor RHS em atributos unitarios e remover triviais/duplicadas */
    FDList g = decompose_unitary(fds);
    if (g.count == 0) {
        return g;
    }

    /*
     * Passo 2: remocao de atributos estranhos no LHS.
     * Um atributo B em X e estranho em (X -> A) se e somente se A continua
     * pertencendo ao fecho de (X \ {B}) calculado sobre o conjunto atual G.
     * Como atualizar X para (X \ {B}) preserva G+, o fecho dos demais testes nao muda.
     */
    for (size_t i = 0; i < g.count; i++) {
        for (int bit = 0; bit < MAX_ATTRS; bit++) {
            AttrMask attr = (AttrMask)(1u << bit);
            if ((g.items[i].lhs & attr) == 0u) {
                continue;
            }

            AttrMask reduced = g.items[i].lhs & ~attr;
            if (reduced == 0u) {
                continue; /* nao reduz para LHS vazio */
            }

            /* Usa G completo (incluindo a propria DF i) conforme a definicao formal */
            AttrMask cl = compute_closure(reduced, &g);
            if ((cl & g.items[i].rhs) != 0u) {
                g.items[i].lhs = reduced; /* atributo confirmado como estranho: remove */
            }
        }
    }

    /*
     * Passo 3: remocao de duplicatas e de DFs redundantes.
     * O vetor `removed` marca exclusoes imediatas sem realocar `g` durante os testes.
     */
    bool *removed = (bool *)calloc(g.count, sizeof(bool));
    if (!removed) {
        fprintf(stderr, "Erro: falha ao alocar memoria na cobertura minima.\n");
        exit(EXIT_FAILURE);
    }

    /*
     * Reduzir o LHS no Passo 2 pode tornar duas DFs identicas (ex: AB->C vira A->C,
     * que ja existia). Removemos as copias extras mantendo a primeira ocorrencia.
     */
    for (size_t i = 0; i < g.count; i++) {
        if (removed[i]) {
            continue;
        }
        for (size_t j = i + 1; j < g.count; j++) {
            if (!removed[j] &&
                g.items[j].lhs == g.items[i].lhs &&
                g.items[j].rhs == g.items[i].rhs) {
                removed[j] = true;
            }
        }
    }

    /*
     * Uma DF fi = (X -> A) e redundante se A ainda pertence a X+ calculado em G \ {fi}.
     * Marcar `removed[i] = true` imediatamente e essencial: em ciclos ou DFs mutuamente
     * redundantes, impede que duas DFs equivalentes sejam removidas ao mesmo tempo.
     */
    for (size_t i = 0; i < g.count; i++) {
        if (removed[i]) {
            continue;
        }
        AttrMask cl = compute_closure_skip(g.items[i].lhs, &g, removed, i);
        if ((cl & g.items[i].rhs) != 0u) {
            removed[i] = true;
        }
    }

    /* Monta a lista final apenas com as DFs que sobreviveram as 3 etapas */
    FDList result;
    fdlist_init(&result);
    for (size_t i = 0; i < g.count; i++) {
        if (!removed[i]) {
            fdlist_push(&result, g.items[i].lhs, g.items[i].rhs);
        }
    }

    free(removed);
    fdlist_free(&g);
    return result;
}

