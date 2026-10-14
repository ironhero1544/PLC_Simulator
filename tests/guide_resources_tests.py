"""Regression checks for the three-language offline guide bundle."""

import importlib.util
import json
import re
import tempfile
import unittest
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "package_guides", ROOT / "scripts/package_guides.py")
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


def code_blocks(text):
    return [(kind, body) for kind, body in
            re.findall(r"^```([^\n]*)\n(.*?)^```", text, re.M | re.S)
            if kind != "mermaid"]


class GuideResourcesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.originals = sorted(
            p for folder in ("program", "exam03")
            for p in (ROOT / "docs/guides" / folder).glob("*.md")
            if not p.stem.endswith((".en", ".ja")))

    def test_translations_preserve_code_and_media(self):
        self.assertTrue(self.originals)
        for source in self.originals:
            original = source.read_text(encoding="utf-8-sig")
            media = re.findall(r"!\[[^\]]*\]\(([^)]+)\)", original)
            for language in ("en", "ja"):
                path = source.with_suffix(f".{language}.md")
                with self.subTest(path=path):
                    translated = path.read_text(encoding="utf-8")
                    self.assertEqual(code_blocks(original), code_blocks(translated))
                    self.assertEqual(media, re.findall(
                        r"!\[[^\]]*\]\(([^)]+)\)", translated))
                    self.assertEqual(len(re.findall(r"^# ", original, re.M)),
                                     len(re.findall(r"^# ", translated, re.M)))
                    prose = re.sub(r"^```.*?^```", "", translated, flags=re.M | re.S)
                    prose = re.sub(r"<[^>]*>|\]\([^)]*\)", "", prose)
                    self.assertIsNone(re.search("[가-힣]", prose), str(path))

    def test_local_links_use_the_document_language(self):
        for source in self.originals:
            for language in ("en", "ja"):
                path = source.with_suffix(f".{language}.md")
                for link in re.findall(r"!?\[[^\]]*\]\(([^)]+)\)",
                                       path.read_text(encoding="utf-8")):
                    url = urlsplit(link.strip("<>"))
                    if url.scheme or not url.path:
                        continue
                    target = (path.parent / unquote(url.path)).resolve()
                    with self.subTest(path=path, link=link):
                        self.assertTrue(target.is_file())
                        if target.suffix == ".md":
                            self.assertTrue(target.stem.endswith(f".{language}"))
                            if url.fragment:
                                self.assertIn(f'id="{unquote(url.fragment)}"',
                                              target.read_text(encoding="utf-8"))

    def test_bundle_contains_all_three_languages(self):
        temporary_root = ROOT / "tmp"
        temporary_root.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="plc-guide-test-",
                                         dir=temporary_root) as temporary:
            output = Path(temporary)
            self.assertTrue(output.resolve().is_relative_to(temporary_root))
            packager.package(ROOT, output)
            script = (output / "documents.js").read_text(encoding="utf-8")
            pages = json.loads(script.split("=", 1)[1].rstrip(";\r\n"))
            self.assertEqual(len(pages), len(self.originals) * 3)
            for language in ("ko", "en", "ja"):
                self.assertEqual(sum(p["language"] == language for p in pages.values()),
                                 len(self.originals))
            for path in pages:
                self.assertTrue((output / path).is_file())


if __name__ == "__main__":
    unittest.main()
