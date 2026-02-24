import dis
import importlib.util
import re
import ast
from pathlib import Path


GS_DIS_PATH = Path(__file__).with_name(".gsdebug") / "demo.opcode.dis"
GS_SOURCE_PATH = Path(__file__).with_name("demo.gs")
PY_DEMO_PATH = Path(__file__).with_name("demo.py")
REPORT_PATH = Path(__file__).with_name("demo_gs_py_instruction_comparison.md")


TARGET_FUNCTIONS = [
    "test_load_const",
    "test_arithmetic_and_string",
    "test_list_and_dict",
    "test_tuple",
    "test_loops_and_control_flow",
    "test_modules_and_imports",
    "test_classes_and_host_objects",
    "benchmark_hot_loop",
    "benchmark_module_calls",
    "benchmark_iter_traversal",
    "benchmark_printf",
    "run_bench",
    "ue_entry",
    "main",
]


def parse_gs_instruction_counts(dis_path: Path):
    counts = {}
    current_func = None
    instruction_line = re.compile(r"^\s*\d+\s+\d+\s+\d+:\d+\s+\S+")
    func_header = re.compile(r"^func\s+([^\(]+)\(")

    for line in dis_path.read_text(encoding="utf-8").splitlines():
        header = func_header.match(line)
        if header:
            current_func = header.group(1)
            counts[current_func] = 0
            continue

        if current_func and instruction_line.match(line):
            counts[current_func] += 1

    return counts


def extract_gs_function_sources(source_path: Path):
    text = source_path.read_text(encoding="utf-8")
    lines = text.splitlines()
    joined = "\n".join(lines)

    header_pattern = re.compile(r"^\s*fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^\)]*\)\s*\{", re.MULTILINE)
    sources = {}

    for match in header_pattern.finditer(joined):
        name = match.group(1)
        start = match.start()
        open_brace_index = joined.find("{", match.end() - 1)
        if open_brace_index == -1:
            continue

        depth = 0
        end_index = None
        for idx in range(open_brace_index, len(joined)):
            ch = joined[idx]
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    end_index = idx
                    break

        if end_index is None:
            continue

        sources[name] = joined[start:end_index + 1].strip()

    return sources


def load_demo_module(py_path: Path):
    spec = importlib.util.spec_from_file_location("demo_py", py_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load demo.py module")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def extract_python_function_sources(py_path: Path):
    text = py_path.read_text(encoding="utf-8")
    tree = ast.parse(text)
    lines = text.splitlines()
    sources = {}

    for node in tree.body:
        if isinstance(node, ast.FunctionDef):
            start = node.lineno - 1
            end = node.end_lineno
            sources[node.name] = "\n".join(lines[start:end]).strip()

    return sources


def count_python_instructions(func):
    return sum(1 for _ in dis.get_instructions(func))


def build_report_markdown(rows, gs_total, py_total, gs_sources, py_sources):
    ratio = (py_total / gs_total) if gs_total else 0.0
    out = []
    out.append("# Demo GS vs Python 指令数对比")
    out.append("")
    out.append(f"- GS 字节码来源: `{GS_DIS_PATH.as_posix()}`")
    out.append(f"- GS 源码: `{GS_SOURCE_PATH.as_posix()}`")
    out.append(f"- Python 源码: `{PY_DEMO_PATH.as_posix()}`")
    out.append("")
    out.append("## 汇总")
    out.append("")
    out.append("| 指标 | 数值 |")
    out.append("|---|---:|")
    out.append(f"| GS 指令总数 | {gs_total} |")
    out.append(f"| Python 指令总数 | {py_total} |")
    out.append(f"| 差值 (Py - GS) | {py_total - gs_total} |")
    out.append(f"| 比例 (Py / GS) | {ratio:.4f} |")
    out.append("")
    out.append("## 函数级对比")
    out.append("")
    out.append("| 函数 | GS 指令数 | Py 指令数 | 差值(Py-GS) |")
    out.append("|---|---:|---:|---:|")
    for name, gs_count, py_count, delta in rows:
        out.append(f"| `{name}` | {gs_count} | {py_count} | {delta} |")

    out.append("")
    out.append("## 左右对比（含函数原代码）")
    out.append("")

    for name, gs_count, py_count, delta in rows:
        gs_code = gs_sources.get(name, "# (未找到对应 GS 函数源码)")
        py_code = py_sources.get(name, "# (未找到对应 Python 函数源码)")

        out.append(f"### `{name}`")
        out.append("")
        out.append(f"- GS 指令数: **{gs_count}**  |  Py 指令数: **{py_count}**  |  差值(Py-GS): **{delta}**")
        out.append("")
        out.append("<table>")
        out.append("  <tr>")
        out.append("    <th align=\"left\" width=\"50%\">GameScript (GS)</th>")
        out.append("    <th align=\"left\" width=\"50%\">Python (Py)</th>")
        out.append("  </tr>")
        out.append("  <tr>")
        out.append("    <td valign=\"top\">")
        out.append("")
        out.append("```gs")
        out.append(gs_code)
        out.append("```")
        out.append("")
        out.append("    </td>")
        out.append("    <td valign=\"top\">")
        out.append("")
        out.append("```python")
        out.append(py_code)
        out.append("```")
        out.append("")
        out.append("    </td>")
        out.append("  </tr>")
        out.append("</table>")
        out.append("")

    out.append("")
    return "\n".join(out)


def main():
    gs_counts = parse_gs_instruction_counts(GS_DIS_PATH)
    gs_sources = extract_gs_function_sources(GS_SOURCE_PATH)
    py_sources = extract_python_function_sources(PY_DEMO_PATH)
    demo_mod = load_demo_module(PY_DEMO_PATH)

    rows = []
    gs_total = 0
    py_total = 0

    for name in TARGET_FUNCTIONS:
        gs_count = gs_counts.get(name, 0)
        func = getattr(demo_mod, name)
        py_count = count_python_instructions(func)

        rows.append((name, gs_count, py_count, py_count - gs_count))
        gs_total += gs_count
        py_total += py_count

    print("Function,GS_Instr,Py_Instr,Delta(Py-GS)")
    for name, gs_count, py_count, delta in rows:
        print(f"{name},{gs_count},{py_count},{delta}")

    ratio = (py_total / gs_total) if gs_total else 0.0
    print(f"TOTAL,{gs_total},{py_total},{py_total - gs_total}")
    print(f"RATIO_PY_OVER_GS,{ratio:.4f}")

    report = build_report_markdown(rows, gs_total, py_total, gs_sources, py_sources)
    REPORT_PATH.write_text(report, encoding="utf-8")
    print(f"REPORT,{REPORT_PATH.as_posix()}")


if __name__ == "__main__":
    main()
