#!/usr/bin/env python3
"""Sanity checks for the translation (.ts) files and the localized resources.

Usage:
    tools/translations-check.py [--root DIR] [--mode static|lupdate-drift]

--mode static (default)
    * RedPandaIDE/translations/*.ts must not contain unfinished entries
    * every %1/%2 placeholder of a <source> must also be present in its <translation>
    * all RedPandaIDE/translations/*.ts must provide the same set of (context, source)
    * every Name/Description/Category of platform/**/info.template must be
      translated for zh_CN, zh_TW, ru_RU and pt_BR

--mode lupdate-drift
    Run *after* `cmake --build build --target update_translations`.
    Fails when lupdate added a <source> that was not in that file before, which
    means a translatable string was added to the sources but never extracted.
    The comparison is per file and ignores the contexts, so that lupdate
    re-grouping, re-ordering or re-numbering messages does not cause noise.

Exit code is 0 when everything is fine, 1 otherwise.
"""

import argparse
import glob
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

LANGS = ("zh_CN", "zh_TW", "ru_RU", "pt_BR")
PLACEHOLDER = re.compile(r"%\d")
TEMPLATE_KEYS = ("Name", "Description", "Category")
ANNOTATE = os.environ.get("GITHUB_ACTIONS") == "true"

errors = []
warnings = []


def report(message, file=None):
    errors.append(message)
    if ANNOTATE and file is not None:
        print(f"::error file={file}::{message}")
    elif ANNOTATE:
        print(f"::error::{message}")
    else:
        print(f"error: {message}" + (f"  [{file}]" if file else ""))


def warn(message):
    warnings.append(message)


def ide_translation_files(root):
    return sorted(glob.glob(os.path.join(root, "RedPandaIDE", "translations", "*.ts")))


def library_translation_files(root):
    return sorted(glob.glob(os.path.join(root, "libs", "*", "*.ts")))


def all_translation_files(root):
    return ide_translation_files(root) + library_translation_files(root)


def parse(path):
    """Return the message list of a .ts file as (context, source, translation)."""
    root = ET.parse(path)
    messages = []
    for context in root.getroot().findall("context"):
        name = context.find("name")
        context_name = name.text if name is not None else ""
        for message in context.findall("message"):
            source = message.find("source")
            translation = message.find("translation")
            if source is None or translation is None:
                continue
            messages.append((context_name, source.text or "", translation))
    return messages


def parse_text(text):
    return parse_from_root(ET.fromstring(text))


def parse_from_root(root):
    messages = []
    for context in root.findall("context"):
        for message in context.findall("message"):
            source = message.find("source")
            translation = message.find("translation")
            if source is None or translation is None:
                continue
            messages.append((source.text or "", translation.text or ""))
    return messages


def is_live(translation):
    return translation.get("type") not in ("vanished", "obsolete")


def check_unfinished(root):
    for path in ide_translation_files(root):
        for _, source, translation in parse(path):
            if translation.get("type") == "unfinished":
                report(f"unfinished translation: {source!r}", relative(root, path))
    for path in library_translation_files(root):
        count = sum(1 for _, _, t in parse(path) if t.get("type") == "unfinished")
        if count:
            warn(f"{relative(root, path)}: {count} unfinished entries (not enforced)")


def check_placeholders(root):
    for path in all_translation_files(root):
        for _, source, translation in parse(path):
            if not is_live(translation) or translation.get("type") == "unfinished":
                continue
            wanted = sorted(set(PLACEHOLDER.findall(source)))
            found = sorted(set(PLACEHOLDER.findall(translation.text or "")))
            if wanted != found:
                report(
                    f"placeholder mismatch for {source!r}: expected {wanted}, "
                    f"got {found}",
                    relative(root, path),
                )


def check_language_parity(root):
    reference_path = None
    reference = set()
    for path in ide_translation_files(root):
        keys = {(c, s) for c, s, t in parse(path) if is_live(t)}
        if reference_path is None:
            reference_path, reference = path, keys
            continue
        name = os.path.basename(reference_path)
        for key in sorted(reference - keys):
            report(
                f"entry of {name} is missing here (context {key[0]!r}, "
                f"source {key[1]!r})",
                relative(root, path),
            )
        for key in sorted(keys - reference):
            report(
                f"entry is not present in {name} (context {key[0]!r}, "
                f"source {key[1]!r})",
                relative(root, path),
            )


def check_project_templates(root):
    pattern = os.path.join(root, "platform", "**", "info.template")
    for path in sorted(glob.glob(pattern, recursive=True)):
        keys = {}
        with open(path, encoding="utf-8") as handle:
            for line in handle:
                match = re.match(r"\s*(Name|Description|Category)(\[(\w+)\])?\s*=", line)
                if not match:
                    continue
                keys.setdefault(match.group(1), set())
                if match.group(3):
                    keys[match.group(1)].add(match.group(3))
        for key in TEMPLATE_KEYS:
            if key not in keys:
                continue
            missing = [lang for lang in LANGS if lang not in keys[key]]
            if missing:
                report(
                    f"{key} is not translated to {', '.join(missing)}",
                    relative(root, path),
                )


def show_at_head(root, relative_path):
    result = subprocess.run(
        ["git", "-C", root, "show", f"HEAD:{relative_path}"],
        capture_output=True,
    )
    if result.returncode != 0:
        return None
    return result.stdout.decode("utf-8")


def check_lupdate_drift(root):
    for path in all_translation_files(root):
        relative_path = relative(root, path)
        before = show_at_head(root, relative_path)
        if before is None:
            continue
        try:
            old = {source for source, _ in parse_text(before)}
            new = {source for source, _ in parse_text(read_text(path))}
        except ET.ParseError as exc:
            report(f"not well formed XML: {exc}", relative_path)
            continue
        for source in sorted(new - old):
            report(
                "translatable string is missing from the .ts file (run "
                f"update_translations): {source!r}",
                relative_path,
            )


def read_text(path):
    with open(path, encoding="utf-8", newline="") as handle:
        return handle.read()


def relative(root, path):
    return os.path.relpath(path, root).replace(os.sep, "/")


def main():
    parser = argparse.ArgumentParser(
        description="check the translation files", epilog=__doc__
    )
    parser.add_argument(
        "--root",
        default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        help="repository root (default: parent of this script's directory)",
    )
    parser.add_argument("--mode", default="static", choices=("static", "lupdate-drift"))
    args = parser.parse_args()
    root = os.path.abspath(args.root)

    if args.mode == "lupdate-drift":
        check_lupdate_drift(root)
    else:
        check_unfinished(root)
        check_placeholders(root)
        check_language_parity(root)
        check_project_templates(root)

    for warning in warnings:
        print(f"warning: {warning}")
        if ANNOTATE:
            print(f"::warning::{warning}")

    if errors:
        print(f"\n{len(errors)} problem(s) found")
        return 1
    print("translation files are consistent")
    return 0


if __name__ == "__main__":
    sys.exit(main())
