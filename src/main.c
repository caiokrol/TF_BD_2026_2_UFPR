#include "fdtool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exibe os subcomandos e argumentos aceitos pela CLI */
static void print_usage(void) {
    fprintf(stderr,
            "Uso:\n"
            "  fdtool closure --fds <arquivo.fds> --X <ATRIBUTOS>\n"
            "  fdtool mincover --fds <arquivo.fds>\n"
            "  fdtool keys --fds <arquivo.fds>\n"
            "  fdtool normalform --fds <arquivo.fds>\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return EXIT_FAILURE;
    }

    const char *cmd = argv[1];
    const char *fds_path = NULL;
    const char *x_arg = NULL;

    /* Le as flags da linha de comando em qualquer ordem (--fds e --X) */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--fds") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Erro: faltou o caminho do arquivo apos '--fds'.\n");
                return EXIT_FAILURE;
            }
            fds_path = argv[++i];
        } else if (strcmp(argv[i], "--X") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Erro: faltou o conjunto de atributos apos '--X'.\n");
                return EXIT_FAILURE;
            }
            x_arg = argv[++i];
        } else {
            fprintf(stderr, "Erro: argumento desconhecido '%s'.\n", argv[i]);
            print_usage();
            return EXIT_FAILURE;
        }
    }

    /* Valida subcomando e parametros obrigatorios */
    if (!fds_path) {
        fprintf(stderr, "Erro: parametro obrigatorio '--fds' nao informado.\n");
        print_usage();
        return EXIT_FAILURE;
    }

    bool is_closure = (strcmp(cmd, "closure") == 0);
    bool is_mincover = (strcmp(cmd, "mincover") == 0);
    bool is_keys = (strcmp(cmd, "keys") == 0);
    bool is_normalform = (strcmp(cmd, "normalform") == 0);

    if (!is_closure && !is_mincover && !is_keys && !is_normalform) {
        fprintf(stderr, "Erro: subcomando desconhecido '%s'.\n", cmd);
        print_usage();
        return EXIT_FAILURE;
    }

    if (is_closure && !x_arg) {
        fprintf(stderr, "Erro: parametro obrigatorio '--X' nao informado para 'closure'.\n");
        print_usage();
        return EXIT_FAILURE;
    }

    /* Carrega o universo U e o conjunto de DFs F do arquivo .fds */
    Schema schema;
    if (!parse_fds_file(fds_path, &schema)) {
        return EXIT_FAILURE;
    }

    /* Despacha para o subcomando solicitado e imprime a saida no formato esperado */
    if (is_closure) {
        AttrMask x_mask = 0u;
        if (!parse_attr_string(x_arg, schema.universe, &x_mask)) {
            schema_free(&schema);
            return EXIT_FAILURE;
        }
        AttrMask result = compute_closure(x_mask, &schema.fds);
        char out[MAX_ATTRS + 1];
        mask_to_str(result, out);
        printf("%s\n", out);
    } else if (is_mincover) {
        FDList mincover = compute_mincover(&schema.fds);
        for (size_t i = 0; i < mincover.count; i++) {
            char lhs_str[MAX_ATTRS + 1];
            char rhs_str[MAX_ATTRS + 1];
            mask_to_str(mincover.items[i].lhs, lhs_str);
            mask_to_str(mincover.items[i].rhs, rhs_str);
            printf("%s->%s\n", lhs_str, rhs_str);
        }
        fdlist_free(&mincover);
    } else if (is_keys) {
        MaskList keys = compute_candidate_keys(schema.universe, &schema.fds);
        for (size_t i = 0; i < keys.count; i++) {
            char key_str[MAX_ATTRS + 1];
            mask_to_str(keys.items[i], key_str);
            printf("%s\n", key_str);
        }
        masklist_free(&keys);
    } else if (is_normalform) {
        check_normal_forms(schema.universe, &schema.fds);
    }

    /* Libera a memoria alocada do esquema antes de encerrar */
    schema_free(&schema);
    return EXIT_SUCCESS;
}
