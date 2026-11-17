#!/usr/bin/env python3
"""Convert a Claude Code JSONL session transcript into readable Markdown.

Usage:
    python claude_jsonl_to_md.py session.jsonl
    python claude_jsonl_to_md.py session.jsonl -o session.md

By default, this exports the main conversation, user/assistant text, tool calls,
and tool results. It deliberately leaves out internal thinking blocks and
Claude Code bookkeeping records (API payloads, cost state, file-history
snapshots, etc.).
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Iterable


def json_fence(value: Any) -> str:
    """Return JSON in a fenced block, using a fence that cannot close early."""
    rendered = json.dumps(value, indent=2, ensure_ascii=False, default=str)
    return fenced(rendered, "json")


def fenced(text: str, language: str = "") -> str:
    """Fence arbitrary text safely, even if it contains triple backticks."""
    longest = 0
    run = 0
    for char in text:
        if char == "`":
            run += 1
            longest = max(longest, run)
        else:
            run = 0
    fence = "`" * max(3, longest + 1)
    return f"{fence}{language}\n{text.rstrip()}\n{fence}\n"


def as_text(value: Any) -> str:
    """Convert text-like tool-result content into a readable string."""
    if value is None:
        return ""
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        pieces: list[str] = []
        for item in value:
            if isinstance(item, str):
                pieces.append(item)
            elif isinstance(item, dict):
                # Tool results sometimes contain text/image/document blocks.
                if isinstance(item.get("text"), str):
                    pieces.append(item["text"])
                elif item.get("type") == "image":
                    pieces.append("[image content omitted]")
                else:
                    pieces.append(json.dumps(item, ensure_ascii=False, default=str))
            else:
                pieces.append(str(item))
        return "\n".join(pieces)
    return json.dumps(value, ensure_ascii=False, default=str)


def get_timestamp(record: dict[str, Any]) -> str:
    """Get the record timestamp without changing its timezone or precision."""
    value = record.get("timestamp")
    return str(value) if value else "Timestamp unavailable"


def render_content_block(
    block: Any,
    role: str,
    *,
    include_tool_inputs: bool,
    include_thinking: bool,
) -> list[str]:
    """Render one Claude message content block as Markdown lines."""
    output: list[str] = []

    if isinstance(block, str):
        if block.strip():
            output.extend([block.rstrip(), ""])
        return output

    if not isinstance(block, dict):
        if block is not None:
            output.extend([str(block), ""])
        return output

    block_type = block.get("type", "unknown")

    if block_type == "text":
        text = block.get("text", "")
        if text:
            output.extend([str(text).rstrip(), ""])

    elif block_type == "tool_use":
        tool_name = block.get("name", "unknown tool")
        output.extend([f"### Tool call: `{tool_name}`", ""])
        if include_tool_inputs and "input" in block:
            output.append(json_fence(block["input"]))
            output.append("")
        else:
            output.append("Tool input omitted.\n")

    elif block_type == "tool_result":
        tool_content = as_text(block.get("content", ""))
        tool_id = block.get("tool_use_id")
        heading = "### Tool result"
        if tool_id:
            heading += f" (`{tool_id}`)"
        if block.get("is_error"):
            heading += " — error"
        output.extend([heading, ""])
        if tool_content:
            # Tool output is fenced to preserve console output and source code.
            output.append(fenced(tool_content, "text"))
            output.append("")
        else:
            output.extend(["*(No tool output)*", ""])

    elif block_type == "thinking":
        if include_thinking:
            thinking = block.get("thinking", "")
            if thinking:
                output.extend(["<details>", "<summary>Thinking block</summary>", "", str(thinking), "", "</details>", ""])

    elif block_type in {"image", "document"}:
        output.extend([f"*[{block_type} content omitted from Markdown export]*", ""])

    else:
        # Preserve visibility of unfamiliar block types instead of silently
        # discarding potentially useful transcript content.
        output.extend([f"*[Unsupported content block: {block_type}]*", ""])

    return output


def iter_records(path: Path) -> Iterable[tuple[int, dict[str, Any]]]:
    """Yield parsed JSONL records with line numbers and useful parse errors."""
    with path.open("r", encoding="utf-8-sig") as source:
        for line_number, raw_line in enumerate(source, start=1):
            if not raw_line.strip():
                continue
            try:
                record = json.loads(raw_line)
            except json.JSONDecodeError as exc:
                raise ValueError(
                    f"Invalid JSON on line {line_number}: {exc.msg} "
                    f"(column {exc.colno})"
                ) from exc
            if not isinstance(record, dict):
                continue
            yield line_number, record


def convert(
    input_path: Path,
    output_path: Path,
    *,
    include_sidechains: bool = False,
    include_tool_inputs: bool = True,
    include_thinking: bool = False,
) -> tuple[int, int]:
    """Convert a Claude Code JSONL session to Markdown.

    Returns (number of exported user/assistant records, number of skipped
    sidechain user/assistant records).
    """
    exported = 0
    skipped_sidechains = 0
    session_id = ""
    cwd = ""
    version = ""
    git_branch = ""

    output_lines: list[str] = ["# Claude Code conversation", ""]

    for _line_number, record in iter_records(input_path):
        # Extract session metadata from any record that carries it.
        session_id = session_id or str(record.get("sessionId", ""))
        cwd = cwd or str(record.get("cwd", ""))
        version = version or str(record.get("version", ""))
        git_branch = git_branch or str(record.get("gitBranch", ""))

        record_type = record.get("type")
        if record_type not in {"user", "assistant"}:
            continue

        if record.get("isSidechain", False) and not include_sidechains:
            skipped_sidechains += 1
            continue

        message = record.get("message", {})
        if not isinstance(message, dict):
            message = {}

        role = "User" if record_type == "user" else "Claude"
        timestamp = get_timestamp(record)
        content = message.get("content", [])

        # Render first so records containing only hidden thinking blocks do
        # not create empty headings in the Markdown transcript.
        body_lines: list[str] = []
        if isinstance(content, str):
            if content.strip():
                body_lines.extend([content.rstrip(), ""])
        elif isinstance(content, list):
            for block in content:
                body_lines.extend(
                    render_content_block(
                        block,
                        role,
                        include_tool_inputs=include_tool_inputs,
                        include_thinking=include_thinking,
                    )
                )
        elif content is not None:
            rendered = as_text(content)
            if rendered.strip():
                body_lines.extend([rendered, ""])

        # Skip records with no user-visible content unless --include-thinking
        # caused a thinking block to be rendered.
        if not any(line.strip() for line in body_lines):
            continue

        output_lines.extend([f"## {role} — {timestamp}", ""])
        output_lines.extend(body_lines)
        exported += 1

    # Add source metadata after processing the records.
    metadata = [
        f"- **Source:** `{input_path.name}`",
    ]
    if session_id:
        metadata.append(f"- **Session ID:** `{session_id}`")
    if version:
        metadata.append(f"- **Claude Code version:** `{version}`")
    if cwd:
        metadata.append(f"- **Working directory:** `{cwd}`")
    if git_branch:
        metadata.append(f"- **Git branch:** `{git_branch}`")

    output_lines[2:2] = metadata + ["", "---", ""]
    output_lines.append("")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(output_lines), encoding="utf-8")
    return exported, skipped_sidechains


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert a Claude Code .jsonl session into readable Markdown."
    )
    parser.add_argument("input", type=Path, help="Path to the Claude Code .jsonl file")
    parser.add_argument(
        "-o", "--output", type=Path,
        help="Output Markdown path (default: same name with .md extension)",
    )
    parser.add_argument(
        "--include-sidechains", action="store_true",
        help="Include subagent/sidechain user and assistant records",
    )
    parser.add_argument(
        "--no-tool-inputs", action="store_true",
        help="Show tool names but omit their JSON input arguments",
    )
    parser.add_argument(
        "--include-thinking", action="store_true",
        help="Include thinking blocks when present (omitted by default)",
    )
    args = parser.parse_args()

    input_path: Path = args.input
    if not input_path.is_file():
        parser.error(f"Input file does not exist: {input_path}")

    output_path: Path = args.output or input_path.with_suffix(".md")

    try:
        exported, skipped = convert(
            input_path,
            output_path,
            include_sidechains=args.include_sidechains,
            include_tool_inputs=not args.no_tool_inputs,
            include_thinking=args.include_thinking,
        )
    except (OSError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    print(f"Markdown transcript written to: {output_path}")
    print(f"Exported user/assistant records: {exported}")
    if skipped:
        print(f"Skipped sidechain records: {skipped} (use --include-sidechains to include)")
    print("Thinking blocks: " + ("included" if args.include_thinking else "omitted"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
