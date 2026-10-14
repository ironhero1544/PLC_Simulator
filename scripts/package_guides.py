"""Bundle the Markdown link/media closure for the offline application reader."""
import argparse
import json
import re
import shutil
from pathlib import Path
from urllib.parse import unquote, urlsplit


def package(root, output):
    source = root / "resources/help"
    output.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, output, dirs_exist_ok=True)
    queue = []
    for folder in ("program", "exam03"):
        originals = sorted((root / "docs/guides" / folder).glob("*.md"))
        originals = [p for p in originals if not p.stem.endswith((".en", ".ja"))]
        for original in originals:
            queue.append(original)
            for language in ("en", "ja"):
                queue.append(original.with_suffix(f".{language}.md"))
    pages = {}
    visited = set()
    while queue:
        path = queue.pop(0).resolve()
        if path in visited:
            continue
        visited.add(path)
        relative = path.relative_to(root).as_posix()
        if not path.is_file():
            raise ValueError(f"Missing guide asset: {relative}")
        destination = output / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
        if path.suffix.lower() != ".md":
            continue
        text = path.read_text(encoding="utf-8-sig")
        title = re.search(r"^#\s+(.+)$", text, re.M)
        language = path.stem.rsplit(".", 1)[-1]
        language = language if language in {"en", "ja"} else "ko"
        pages[relative] = {"title": title[1] if title else path.stem,
                           "language": language, "markdown": text}
        for link in re.findall(r"!?\[[^\]]*\]\(([^)]+)\)", text):
            link = link.strip().split(' "')[0].strip("<>")
            url = urlsplit(link)
            if url.scheme or url.netloc or not url.path:
                continue
            target = (path.parent / unquote(url.path)).resolve()
            target.relative_to(root)  # Reject paths outside the source tree.
            if target.suffix.lower() in {".md", ".png", ".gif", ".jpg", ".jpeg", ".svg", ".webp", ".json"}:
                queue.append(target)
    payload = json.dumps(pages, ensure_ascii=False).replace("<", "\\u003c")
    (output / "documents.js").write_text("window.GUIDE_DOCUMENTS=" + payload + ";\n", encoding="utf-8")
    print(f"Packaged {len(pages)} Markdown documents, {len(visited) - len(pages)} media files")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    package(args.root.resolve(), args.output.resolve())
