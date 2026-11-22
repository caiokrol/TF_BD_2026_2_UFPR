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


/*
 * Dada uma superchave `key` (key+ == universe), tenta remover cada atributo
 * individualmente de forma gulosa: se o conjunto sem aquele atributo continuar
 * determinando todo o universo U, ele era redundante e e descartado.
 * Ao final, nenhum subconjunto proprio determina U -> resultado e chave candidata.
 */
static AttrMask minimize_superkey(AttrMask key, AttrMask universe, const FDList *fds) {
    for (int bit = 0; bit < MAX_ATTRS; bit++) {
        AttrMask attr = (AttrMask)(1u << bit);
        if ((key & attr) != 0u) {
            AttrMask candidate = key & ~attr;
            if (compute_closure(candidate, fds) == universe) {
                key = candidate;
            }
        }
    }
    return key;
}

/* Comparador para qsort: ordena chaves candidatas lexicograficamente (ex: AC antes de BD) */
static int compare_masks(const void *a, const void *b) {
    char sa[MAX_ATTRS + 1];
    char sb[MAX_ATTRS + 1];
    mask_to_str(*(const AttrMask *)a, sa);
    mask_to_str(*(const AttrMask *)b, sb);
    return strcmp(sa, sb);
}

/*
 * Encontra TODAS as chaves candidatas do esquema usando o algoritmo de
 * Lucchesi-Osborn (1978), evitando busca exaustiva sobre todos os 2^|U| subconjuntos.
 *
 * Ideia teorica:
 * 1) Partimos de U (sempre superchave) e minimizamos para obter uma primeira chave K0.
 * 2) Para cada chave K ja descoberta e cada DF (X -> Y) em F com K n Y != vazio:
 *    formamos S = X U (K \ Y). S e sempre superchave, pois X deriva Y, e Y com (K \ Y)
 *    recupera K, que por sua vez deriva todo U.
 * 3) Se S nao contiver nenhuma chave ja conhecida, minimizamos S para obter uma nova
 *    chave candidata garantidamente inedita e a adicionamos na fila `keys`.
 */
MaskList compute_candidate_keys(AttrMask universe, const FDList *fds) {
    MaskList keys;
    masklist_init(&keys);

    if (universe == 0u) {
        return keys;
    }

    /* Passo 1: obtem a primeira chave candidata minimizando o universo completo U */
    AttrMask first_key = minimize_superkey(universe, universe, fds);
    masklist_push(&keys, first_key);

    /*
     * Passo 2: `keys` funciona como uma fila crescente (keys.count aumenta dentro do laco).
     * Cada chave descoberta e combinada com todas as DFs ate nenhuma chave nova surgir.
     */
    for (size_t i = 0; i < keys.count; i++) {
        AttrMask k = keys.items[i];

        for (size_t j = 0; j < fds->count; j++) {
            AttrMask lhs = fds->items[j].lhs;
            AttrMask rhs = fds->items[j].rhs;

            /* Otimizacao: se Y nao toca K, entao (K \ Y) == K e S conteria K */
            if ((k & rhs) == 0u) {
                continue;
            }

            /* Substitui os atributos de Y presentes em K pelo determinante X */
            AttrMask s = lhs | (k & ~rhs);

            /*
             * Testa se alguma chave conhecida ja e subconjunto de S.
             * Se for, minimizar S nao garante chave nova (ou gera duplicata).
             * Se nao for, existe necessariamente uma chave candidata inedita dentro de S.
             */
            bool has_known_subset = false;
            for (size_t m = 0; m < keys.count; m++) {
                if ((s & keys.items[m]) == keys.items[m]) {
                    has_known_subset = true;
                    break;
                }
            }

            if (!has_known_subset) {
                AttrMask new_key = minimize_superkey(s, universe, fds);
                masklist_push(&keys, new_key);
            }
        }
    }

    /* Ordena as chaves em ordem alfabetica para saida deterministica */
    if (keys.count > 1) {
        qsort(keys.items, keys.count, sizeof(AttrMask), compare_masks);
    }

    return keys;
}


/*
 * Verifica se o esquema (universe, fds) satisfaz BCNF e 3FN e imprime o diagnostico.
 *
 * Definicoes aplicadas para cada DF nao trivial de RHS unitario (X -> A):
 * - BCNF exige: X e superchave (X+ == U).
 * - 3FN  exige: X e superchave OU A e atributo primo (pertence a alguma chave candidata).
 *
 * Teorema da suficiencia: testar as DFs decompostas de F (ignorando triviais) e
 * equivalente a testar todo o fecho F+.
 */
void check_normal_forms(AttrMask universe, const FDList *fds) {
    /* 1) Encontra todas as chaves candidatas para determinar os atributos primos */
    MaskList keys = compute_candidate_keys(universe, fds);

    /* Um atributo e primo se aparece em pelo menos uma chave candidata (uniao via OR) */
    AttrMask prime_attrs = 0u;
    for (size_t i = 0; i < keys.count; i++) {
        prime_attrs |= keys.items[i];
    }

    /* 2) Decompoe F em DFs nao triviais de RHS unitario sem duplicatas */
    FDList unitary = decompose_unitary(fds);
    FDList bcnf_violations;
    FDList nf3_violations;
    fdlist_init(&bcnf_violations);
    fdlist_init(&nf3_violations);

    /* 3) Classifica cada DF quanto a BCNF e 3FN */
    for (size_t i = 0; i < unitary.count; i++) {
        AttrMask lhs = unitary.items[i].lhs;
        AttrMask rhs = unitary.items[i].rhs;

        /* X e superchave sse o fecho de X sob F determina todo o universo U */
        bool is_superkey = (compute_closure(lhs, fds) == universe);
        if (!is_superkey) {
            /* Se X nao e superchave (e a DF e nao trivial), ja viola BCNF */
            fdlist_push(&bcnf_violations, lhs, rhs);

            /* Como BCNF => 3FN, so pode haver violacao de 3FN quando ja violou BCNF */
            bool is_prime = (prime_attrs & rhs) != 0u;
            if (!is_prime) {
                fdlist_push(&nf3_violations, lhs, rhs);
            }
        }
    }

    /* 4) Imprime status geral seguido do detalhamento no formato exato do enunciado */
    printf("BCNF: %s\n", (bcnf_violations.count == 0) ? "OK" : "VIOLATIONS");
    printf("3NF: %s\n", (nf3_violations.count == 0) ? "OK" : "VIOLATIONS");

    for (size_t i = 0; i < bcnf_violations.count; i++) {
        char lhs_str[MAX_ATTRS + 1];
        char rhs_str[MAX_ATTRS + 1];
        mask_to_str(bcnf_violations.items[i].lhs, lhs_str);
        mask_to_str(bcnf_violations.items[i].rhs, rhs_str);
        printf("VIOLATION BCNF: %s->%s (%s not superkey)\n", lhs_str, rhs_str, lhs_str);
    }

    for (size_t i = 0; i < nf3_violations.count; i++) {
        char lhs_str[MAX_ATTRS + 1];
        char rhs_str[MAX_ATTRS + 1];
        mask_to_str(nf3_violations.items[i].lhs, lhs_str);
        mask_to_str(nf3_violations.items[i].rhs, rhs_str);
        printf("VIOLATION 3NF: %s->%s (%s not prime, %s not superkey)\n",
               lhs_str, rhs_str, rhs_str, lhs_str);
    }

    /* Libera todas as estruturas auxiliares */
    fdlist_free(&nf3_violations);
    fdlist_free(&bcnf_violations);
    fdlist_free(&unitary);
    masklist_free(&keys);
}
