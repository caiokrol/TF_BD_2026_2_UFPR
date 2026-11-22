#!/usr/bin/env python3
"""Script simples para automatizar os testes de `fdtool normalform` e gerar um relatorio."""

from datetime import datetime
from pathlib import Path
import subprocess
import sys


def normalize_lines(text: str) -> list[str]:
    return [line.strip() for line in text.replace("\r\n", "\n").splitlines() if line.strip()]


def compare_outputs(expected: str, actual: str) -> tuple[bool, str]:
    exp_lines = normalize_lines(expected)
    act_lines = normalize_lines(actual)

    if exp_lines == act_lines:
        return True, "identica"

    # Conforme README: se a ordem das violacoes diferir, compara status + conjunto de violacoes
    if len(exp_lines) >= 2 and len(act_lines) >= 2:
        if exp_lines[:2] == act_lines[:2] and sorted(exp_lines[2:]) == sorted(act_lines[2:]):
            return True, "equivalente (ordem diferente)"

    return False, "divergente"


def find_binary(root_dir: Path) -> Path:
    candidates = [root_dir / "fdtool.exe", root_dir / "fdtool"]
    for cand in candidates:
        if cand.is_file():
            return cand
    return candidates[0]


def main() -> int:
    tests_dir = Path(__file__).resolve().parent
    root_dir = tests_dir.parent
    binary = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else find_binary(root_dir)

    if not binary.is_file():
        print(f"Erro: executavel nao encontrado em '{binary}'. Compile o projeto antes (make / mingw32-make).")
        return 1

    fds_files = sorted(tests_dir.glob("*.fds"))
    if not fds_files:
        print(f"Nenhum arquivo .fds encontrado em '{tests_dir}'.")
        return 1

    results = []
    passed = 0

    print(f"Executando {len(fds_files)} testes com: {binary.name}\n")

    for fds_path in fds_files:
        expected_path = fds_path.with_suffix(".expected.txt")
        case_name = fds_path.stem

        if not expected_path.is_file():
            print(f"[SKIP] {case_name} (arquivo {expected_path.name} ausente)")
            continue

        expected_text = expected_path.read_text(encoding="utf-8")
        fds_text = fds_path.read_text(encoding="utf-8").strip()

        proc = subprocess.run(
            [str(binary), "normalform", "--fds", str(fds_path)],
            capture_output=True,
            text=True,
        )

        ok, detail = compare_outputs(expected_text, proc.stdout)
        if proc.returncode != 0:
            ok = False
            detail = f"exit code {proc.returncode}"

        if ok:
            passed += 1

        status = "PASS" if ok else "FAIL"
        print(f"[{status}] {case_name} ({detail})")

        results.append({
            "name": case_name,
            "ok": ok,
            "status": status,
            "detail": detail,
            "returncode": proc.returncode,
            "fds": fds_text,
            "expected": expected_text.strip(),
            "actual": proc.stdout.strip(),
            "stderr": proc.stderr.strip(),
        })

    total = len(results)
    failed = total - passed
    print(f"\nResumo: {passed}/{total} passaram ({failed} falharam).")

    # Gerar relatorio em Markdown
    report_path = tests_dir / "relatorio_testes.md"
    now = datetime.now().strftime("%d/%m/%Y %H:%M:%S")

    lines = [
        "# Relatório de Testes — `fdtool normalform`",
        "",
        f"- **Data da execução:** {now}",
        f"- **Executável:** `{binary.name}`",
        f"- **Resultado geral:** **{passed}/{total}** aprovados ({failed} falha(s))",
        "",
        "## Resumo",
        "",
        "| Caso | Status | Comparação | Exit Code |",
        "|---|---|---|---|",
    ]

    for r in results:
        lines.append(f"| `{r['name']}` | **{r['status']}** | {r['detail']} | `{r['returncode']}` |")

    lines.extend(["", "## Detalhes por Caso", ""])

    for r in results:
        lines.extend([
            f"### `{r['name']}` — {r['status']}",
            "",
            "**Entrada (`.fds`):**",
            "```text",
            r["fds"],
            "```",
            "",
            "**Saída esperada:**",
            "```text",
            r["expected"],
            "```",
            "",
            "**Saída obtida:**",
            "```text",
            r["actual"] if r["actual"] else "(vazio)",
            "```",
        ])
        if r["stderr"]:
            lines.extend([
                "",
                "**Stderr:**",
                "```text",
                r["stderr"],
                "```",
            ])
        lines.append("")

    report_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"Relatorio salvo em: {report_path}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
