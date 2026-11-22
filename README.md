# fdtool — Ferramenta para Dependências Funcionais

Implementação em linguagem C (C11) de uma ferramenta de linha de comando para análise de Dependências Funcionais (DFs) em esquemas relacionais com atributos no intervalo `A..Z`.

## Estrutura do Projeto

```text
.
├── Makefile
├── README.md
├── relatorio.pdf
├── src/
│   ├── fdtool.h        # Estruturas de dados e protótipos
│   ├── parser.c        # Leitura e validação de arquivos .fds e máscaras de bits
│   ├── algorithms.c    # Fecho X+, cobertura mínima, chaves candidatas e BCNF/3FN
│   └── main.c          # Interface de linha de comando (CLI)
└── exemplos/
    ├── e1.fds
    ├── e2.fds
    ├── e3.fds
    ├── e4_3nf_not_bcnf.fds
    ├── e5_mincover_chaves.fds
    └── e6_bcnf.fds
└── tests_normalform/
    ├── ...
    └── run_tests.py
```

## Compilação

O projeto não utiliza bibliotecas externas e compila com `gcc -std=c11 -Wall -Wextra`:

```bash
make
```

No Windows com MinGW:

```powershell
mingw32-make
```

Para remover os arquivos objetos e o executável:

```bash
make clean
```

## Formato do Arquivo de Entrada (`.fds`)

Os arquivos definem o universo de atributos `U` e o conjunto de dependências funcionais `F`. Espaços são ignorados e as DFs podem ser separadas por vírgula (`,`) ou ponto e vírgula (`;`).

```text
U = {A,B,C,D,E}
F = { A->BC, C->D, BD->E }
```

## Uso e Exemplos

### 1. Fecho de atributos ($X^+$)

```bash
./fdtool closure --fds exemplos/e1.fds --X A
```

Saída:
```text
ABCDE
```

```bash
./fdtool closure --fds exemplos/e1.fds --X BD
```

Saída:
```text
BDE
```

### 2. Cobertura mínima

```bash
./fdtool mincover --fds exemplos/e1.fds
```

Saída:
```text
A->B
A->C
C->D
BD->E
```

### 3. Chaves candidatas

```bash
./fdtool keys --fds exemplos/e2.fds
```

Saída:
```text
AC
```

### 4. Verificação de BCNF e 3FN

```bash
./fdtool normalform --fds exemplos/e3.fds
```

Saída:
```text
BCNF: VIOLATIONS
3NF: VIOLATIONS
VIOLATION BCNF: B->C (B not superkey)
VIOLATION 3NF: B->C (C not prime, B not superkey)
```

Exemplo em 3FN que viola BCNF (`exemplos/e4_3nf_not_bcnf.fds`):

```bash
./fdtool normalform --fds exemplos/e4_3nf_not_bcnf.fds
```

Saída:
```text
BCNF: VIOLATIONS
3NF: OK
VIOLATION BCNF: C->B (C not superkey)
```
