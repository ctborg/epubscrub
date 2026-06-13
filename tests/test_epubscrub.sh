#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

test -f "$ROOT/man/epubscrub.1"
grep -q '^\.Sh SANITIZATION POLICY' "$ROOT/man/epubscrub.1"
grep -q '^\.It 1$' "$ROOT/man/epubscrub.1"
"$ROOT/epubscrub" --version > "$TMP/version.txt"
grep -q '^epubscrub 0\.3\.1-dev$' "$TMP/version.txt"

mkdir -p "$TMP/book/META-INF" "$TMP/book/OEBPS"
printf 'application/epub+zip' > "$TMP/book/mimetype"
cat > "$TMP/book/META-INF/container.xml" <<'XML'
<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>
XML
cat > "$TMP/book/OEBPS/content.opf" <<'XML'
<?xml version="1.0"?>
<package version="3.0" xmlns="http://www.idpf.org/2007/opf">
  <manifest>
    <item id="chap" href="chapter.xhtml" media-type="application/xhtml+xml"/>
    <item id="bad" href="evil.js" media-type="application/javascript"/>
    <item id="style" href="style.css" media-type="text/css"/>
    <item id="font" href="font.woff" media-type="font/woff"/>
    <item id="svg" href="image.svg" media-type="image/svg+xml"/>
  </manifest>
  <spine><itemref idref="chap"/></spine>
</package>
XML
cat > "$TMP/book/OEBPS/chapter.xhtml" <<'XML'
<?xml version="1.0"?>
<html xmlns="http://www.w3.org/1999/xhtml">
<head><script>alert(1)</script><link rel="stylesheet" href="style.css"/><link rel="stylesheet" href="https://example.invalid/evil.css"/></head>
<body onload="evil()"><p>hello</p><a href="../chapter2.xhtml#top">next</a><a href="https://example.invalid/page">site</a><img src="https://example.invalid/tracker.png"/><iframe src="https://example.invalid"></iframe></body>
</html>
XML
cat > "$TMP/book/OEBPS/style.css" <<'CSS'
@import url("https://example.invalid/style.css");
body { background: url(javascript:alert(1)); color: black; }
CSS
printf 'alert(1)' > "$TMP/book/OEBPS/evil.js"
printf 'fakefont' > "$TMP/book/OEBPS/font.woff"
cat > "$TMP/book/OEBPS/image.svg" <<'SVG'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><circle cx="5" cy="5" r="4"/></svg>
SVG

(cd "$TMP/book" && python3 - <<'PY'
import zipfile
with zipfile.ZipFile("../dirty.epub", "w") as z:
    z.write("mimetype", compress_type=zipfile.ZIP_STORED)
    for name in [
        "META-INF/container.xml",
        "OEBPS/content.opf",
        "OEBPS/chapter.xhtml",
        "OEBPS/style.css",
        "OEBPS/evil.js",
        "OEBPS/font.woff",
        "OEBPS/image.svg",
    ]:
        z.write(name, compress_type=zipfile.ZIP_DEFLATED)
PY
)

python3 - "$TMP/dirs.epub" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1], "w") as z:
    z.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_STORED)
    for name in ["META-INF/", "fonts/", "images/", "text/"]:
        z.writestr(name, "", compress_type=zipfile.ZIP_STORED)
    z.writestr("META-INF/container.xml", """<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles><rootfile full-path="content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>""")
    z.writestr("content.opf", """<?xml version="1.0"?>
<package version="3.0" xmlns="http://www.idpf.org/2007/opf">
  <manifest><item id="chap" href="text/chapter.xhtml" media-type="application/xhtml+xml"/></manifest>
  <spine><itemref idref="chap"/></spine>
</package>""")
    z.writestr("text/chapter.xhtml", """<?xml version="1.0"?>
<html xmlns="http://www.w3.org/1999/xhtml"><body><p>clean</p></body></html>""")
PY

python3 - "$TMP/numeric-xhtml.epub" <<'PY'
import sys, zipfile
chapter = """<?xml version="1.0"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" epub:prefix="daisy: http://www.daisy.org/z3998/2012/vocab/structure/" lang="en-US" xml:lang="en-us">
<head>
  <title>2</title>
  <link href="hcus_hcp-epub.css" rel="stylesheet" type="text/css"/>
  <meta content="urn:uuid:1c432735-7781-40a8-8873-7d188c58f66d" name="Adept.expected.resource"/>
</head>
<body epub:type="bodymatter">
  <section aria-labelledby="to6" epub:type="chapter" role="doc-chapter">
    <h1 id="to6"><a href="nav.xhtml#nto6">2</a></h1>
    <p>Clean chapter text.</p>
  </section>
  <div><p><a href="https://oceanofpdf.com"><i>OceanofPDF.com</i></a></p></div>
</body>
</html>"""
with zipfile.ZipFile(sys.argv[1], "w") as z:
    z.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_STORED)
    z.writestr("META-INF/container.xml", """<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles><rootfile full-path="content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>""")
    z.writestr("content.opf", """<?xml version="1.0"?>
<package version="3.0" xmlns="http://www.idpf.org/2007/opf">
  <manifest>
    <item id="chap2" href="2.xhtml" media-type="application/xhtml+xml"/>
    <item id="style" href="hcus_hcp-epub.css" media-type="text/css"/>
  </manifest>
  <spine><itemref idref="chap2"/></spine>
</package>""")
    z.writestr("2.xhtml", chapter)
    z.writestr("hcus_hcp-epub.css", "body { color: black; }")
PY

set +e
"$ROOT/epubscrub" "$TMP/numeric-xhtml.epub" -o "$TMP/numeric-xhtml-clean.epub" > "$TMP/numeric-xhtml.txt"
code=$?
set -e
test "$code" -eq 1
grep -q 'status: sanitized' "$TMP/numeric-xhtml.txt"
! grep -q 'remove file: 2.xhtml' "$TMP/numeric-xhtml.txt"
grep -q 'sanitize: 2.xhtml (line:' "$TMP/numeric-xhtml.txt"
python3 - "$TMP/numeric-xhtml-clean.epub" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    names = set(z.namelist())
    assert "2.xhtml" in names
    xhtml = z.read("2.xhtml").decode()
    assert 'epub:prefix="daisy: http://www.daisy.org/z3998/2012/vocab/structure/"' in xhtml
    assert 'href="https://oceanofpdf.com"' not in xhtml
    assert 'OceanofPDF.com' not in xhtml
PY

python3 - "$TMP/bad-mimetype-order.epub" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1], "w") as z:
    z.writestr("META-INF/container.xml", """<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles><rootfile full-path="content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>""")
    z.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_DEFLATED)
    z.writestr("content.opf", """<?xml version="1.0"?>
<package version="3.0" xmlns="http://www.idpf.org/2007/opf">
  <manifest><item id="chap" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest>
  <spine><itemref idref="chap"/></spine>
</package>""")
    z.writestr("chapter.xhtml", """<?xml version="1.0"?>
<html xmlns="http://www.w3.org/1999/xhtml"><body><p>clean</p></body></html>""")
PY

set +e
"$ROOT/epubscrub" --dryrun "$TMP/bad-mimetype-order.epub" > "$TMP/bad-mimetype-order-check.txt"
code=$?
set -e
test "$code" -eq 2
grep -q 'reject: mimetype is not the first ZIP entry' "$TMP/bad-mimetype-order-check.txt"

set +e
"$ROOT/epubscrub" --fix "$TMP/bad-mimetype-order.epub" -o "$TMP/fixed-mimetype-order.epub" > "$TMP/fixed-mimetype-order.txt"
code=$?
set -e
test "$code" -eq 1
grep -q 'fix: mimetype will be written as the first ZIP entry' "$TMP/fixed-mimetype-order.txt"
grep -q 'fix: mimetype will be written uncompressed' "$TMP/fixed-mimetype-order.txt"
python3 - "$TMP/fixed-mimetype-order.epub" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    infos = z.infolist()
    assert infos[0].filename == "mimetype"
    assert infos[0].compress_type == zipfile.ZIP_STORED
    assert z.read("mimetype") == b"application/epub+zip"
PY

set +e
"$ROOT/epubscrub" --dryrun "$TMP/dirs.epub" > "$TMP/dirs-check.txt"
code=$?
set -e
test "$code" -eq 0
grep -q 'status: clean' "$TMP/dirs-check.txt"
! grep -q 'remove file: META-INF/' "$TMP/dirs-check.txt"
! grep -q 'remove file: fonts/' "$TMP/dirs-check.txt"
! grep -q 'remove file: images/' "$TMP/dirs-check.txt"
! grep -q 'remove file: text/' "$TMP/dirs-check.txt"

set +e
"$ROOT/epubscrub" "$TMP/dirs.epub" -o "$TMP/dirs-copy.epub" > "$TMP/dirs-copy.txt"
code=$?
set -e
test "$code" -eq 0
test -f "$TMP/dirs-copy.epub"
grep -q 'status: clean' "$TMP/dirs-copy.txt"
python3 - "$TMP/dirs-copy.epub" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    infos = z.infolist()
    assert infos[0].filename == "mimetype"
    assert infos[0].compress_type == zipfile.ZIP_STORED
PY

set +e
"$ROOT/epubscrub" "$TMP/dirs.epub" -o "$TMP/dirs.epub" > "$TMP/same-path.txt" 2>&1
code=$?
set -e
test "$code" -eq 3
grep -q 'output path must not be the same as input path' "$TMP/same-path.txt"

set +e
"$ROOT/epubscrub" "$TMP/dirty.epub" -o "$TMP/clean.epub" --report "$TMP/report.txt"
code=$?
set -e
test "$code" -eq 1
test -f "$TMP/clean.epub"
grep -q 'remove file: OEBPS/evil.js' "$TMP/report.txt"
grep -q 'sanitize: OEBPS/chapter.xhtml' "$TMP/report.txt"
grep -q 'sanitize: OEBPS/style.css' "$TMP/report.txt"
grep -q 'remove file: OEBPS/evil.js (reason: JavaScript is active content' "$TMP/report.txt"
grep -q 'sanitize: OEBPS/chapter.xhtml (line: 3, reason: removed active markup' "$TMP/report.txt"
grep -q 'sanitize: OEBPS/style.css (line: 1, reason: removed remote imports' "$TMP/report.txt"

set +e
"$ROOT/epubscrub" --dryrun "$TMP/dirty.epub" > "$TMP/dirty-check.txt"
code=$?
set -e
test "$code" -eq 1
grep -q 'status: sanitized' "$TMP/dirty-check.txt"
grep -q 'policy: wary' "$TMP/dirty-check.txt"
grep -q 'summary: files_scanned=' "$TMP/dirty-check.txt"
grep -q 'remove file: OEBPS/evil.js (reason: JavaScript is active content' "$TMP/dirty-check.txt"

set +e
"$ROOT/epubscrub" "$TMP/dirty.epub" --report "$TMP/report-only.txt"
code=$?
set -e
test "$code" -eq 1
test -f "$TMP/report-only.txt"
grep -q 'status: sanitized' "$TMP/report-only.txt"
test ! -f "$TMP/dirty.clean.epub"

set +e
"$ROOT/epubscrub" --dryrun --calm "$TMP/dirty.epub" > "$TMP/calm-check.txt"
code=$?
set -e
test "$code" -eq 1
grep -q 'policy: calm' "$TMP/calm-check.txt"

set +e
"$ROOT/epubscrub" --dryrun --paranoid "$TMP/dirty.epub" > "$TMP/paranoid-check.txt"
code=$?
set -e
test "$code" -eq 1
grep -q 'policy: paranoid' "$TMP/paranoid-check.txt"
grep -q 'remove file: OEBPS/font.woff (reason: paranoid policy removes' "$TMP/paranoid-check.txt"
grep -q 'remove file: OEBPS/image.svg (reason: paranoid policy removes' "$TMP/paranoid-check.txt"

set +e
"$ROOT/epubscrub" --dryrun --policy paranoid --report-format json "$TMP/dirty.epub" > "$TMP/report.json"
code=$?
set -e
test "$code" -eq 1
python3 - "$TMP/report.json" <<'PY'
import json, sys
data = json.load(open(sys.argv[1]))
assert data["status"] == "sanitized"
assert data["policy"] == "paranoid"
assert data["summary"]["files_removed"] >= 3
assert any("font.woff" in event for event in data["events"])
assert any("chapter.xhtml" in event and "line: 3" in event for event in data["events"])
PY

python3 - "$TMP/clean.epub" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    names = set(z.namelist())
    assert "OEBPS/evil.js" not in names
    xhtml = z.read("OEBPS/chapter.xhtml").decode()
    css = z.read("OEBPS/style.css").decode()
    opf = z.read("OEBPS/content.opf").decode()
    assert "<script" not in xhtml.lower()
    assert "onload" not in xhtml.lower()
    assert "<iframe" not in xhtml.lower()
    assert "evil.css" not in xhtml.lower()
    assert "tracker.png" not in xhtml.lower()
    assert "../chapter2.xhtml#top" in xhtml
    assert "https://example.invalid/page" not in xhtml
    assert "@import" not in css.lower()
    assert "javascript:" not in css.lower()
    assert "evil.js" not in opf
    assert "OEBPS/font.woff" in names
    assert "OEBPS/image.svg" in names
PY

set +e
"$ROOT/epubscrub" "$TMP/dirty.epub" --paranoid -o "$TMP/paranoid.epub" > "$TMP/paranoid-write.txt"
code=$?
set -e
test "$code" -eq 1
python3 - "$TMP/paranoid.epub" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    names = set(z.namelist())
    xhtml = z.read("OEBPS/chapter.xhtml").decode()
    opf = z.read("OEBPS/content.opf").decode()
    assert "OEBPS/font.woff" not in names
    assert "OEBPS/image.svg" not in names
    assert "https://example.invalid/page" not in xhtml
    assert "font.woff" not in opf
    assert "image.svg" not in opf
PY

set +e
"$ROOT/epubscrub" --dryrun "$TMP/clean.epub" > "$TMP/check.txt"
code=$?
set -e
test "$code" -eq 0
grep -q 'status: clean' "$TMP/check.txt"

printf 'not readable' > "$TMP/noaccess.epub"
chmod 000 "$TMP/noaccess.epub"
if ! head -c 1 "$TMP/noaccess.epub" >/dev/null 2>&1; then
    set +e
    "$ROOT/epubscrub" --dryrun "$TMP/noaccess.epub" > "$TMP/noaccess.txt" 2>&1
    code=$?
    set -e
    test "$code" -eq 3
    grep -q "could not read input file '$TMP/noaccess.epub':" "$TMP/noaccess.txt"
fi
chmod 600 "$TMP/noaccess.epub"

python3 - "$TMP/traversal.epub" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1], "w") as z:
    z.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_STORED)
    z.writestr("../evil.txt", "bad")
PY

set +e
"$ROOT/epubscrub" --dryrun "$TMP/traversal.epub" > "$TMP/traversal.txt" 2>&1
code=$?
set -e
test "$code" -eq 2
grep -q 'unsafe path' "$TMP/traversal.txt"

echo "tests passed"
