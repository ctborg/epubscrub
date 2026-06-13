#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cc -std=c11 -Wall -Wextra -Wpedantic -I"$ROOT/include" \
    -o "$TMP/markup_harness" "$ROOT/tests/markup_harness.c" "$ROOT/src/markup.c"

run_case() {
    policy=$1
    input=$2
    expected=$3
    printf '%s' "$input" | "$TMP/markup_harness" "$policy" > "$TMP/out"
    printf '%s' "$expected" > "$TMP/expected"
    cmp "$TMP/expected" "$TMP/out"
}

run_case wary \
    '<p title="1 > 0" onclick="evil()">math</p>' \
    '<p title="1 > 0">math</p>'

run_case wary \
    '<script>if (a > b) alert(1)</script><p>ok</p>' \
    '<p>ok</p>'

run_case wary \
    '<SCRIPT SRC=x></SCRIPT><p>ok</p>' \
    '<p>ok</p>'

run_case wary \
    '<img src="https://example.invalid/tracker.png" alt="x">' \
    '<img alt="x">'

run_case wary \
    '<img src=https://example.invalid/tracker.gif alt=x />' \
    '<img alt=x />'

run_case wary \
    '<link rel="stylesheet" href="https://example.invalid/style.css">' \
    '<link rel="stylesheet">'

run_case wary \
    '<svg><image xlink:href="https://example.invalid/tracker.png" width="1" /></svg>' \
    '<svg><image width="1" /></svg>'

run_case wary \
    '<a href="../chapter.xhtml#top">next</a><a href="https://example.invalid/page">site</a>' \
    '<a href="../chapter.xhtml#top">next</a><a>site</a>'

run_case wary \
    '<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" epub:prefix="daisy: http://www.daisy.org/z3998/2012/vocab/structure/"><head><link href="local.css" rel="stylesheet" type="text/css"/></head><body epub:type="bodymatter"><a href="https://oceanofpdf.com"><i>OceanofPDF.com</i></a></body></html>' \
    '<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" epub:prefix="daisy: http://www.daisy.org/z3998/2012/vocab/structure/"><head><link href="local.css" rel="stylesheet" type="text/css" /></head><body epub:type="bodymatter"><a><i>OceanofPDF.com</i></a></body></html>'

run_case wary \
    '<p>text</p><div style="float: none; margin: 10px 0px 10px 0px; text-align: center;"><p><a href="https://oceanofpdf.com"><i>OceanofPDF.com</i></a></p></div>' \
    '<p>text</p>'

python3 - "$TMP/large.xhtml" "$TMP/large.expected" <<'PY'
import sys
body = "<p>Clean chapter text.</p>" * 4000
text = '<html xmlns="http://www.w3.org/1999/xhtml"><body>' + body + '</body></html>'
open(sys.argv[1], "w").write(text)
open(sys.argv[2], "w").write(text)
PY
"$TMP/markup_harness" wary < "$TMP/large.xhtml" > "$TMP/large.out"
cmp "$TMP/large.expected" "$TMP/large.out"

run_case paranoid \
    '<a href="../chapter.xhtml#top">next</a><a href="https://example.invalid/page">site</a>' \
    '<a href="../chapter.xhtml#top">next</a><a>site</a>'

run_case wary \
    '<input value="secret"><textarea>secret</textarea><button>go</button><select><option>x</option></select><p>ok</p>' \
    '<p>ok</p>'

run_case wary \
    '<p>before</p><iframe src="https://example.invalid/embed"/><p>after</p>' \
    '<p>before</p><p>after</p>'

run_case wary \
    '<!-- <script>alert(1)</script> --><p>ok</p>' \
    '<!-- <script>alert(1)</script> --><p>ok</p>'

run_case wary \
    '<svg><a xlink:href="javascript:alert(1)">x</a></svg>' \
    '<svg><a>x</a></svg>'

run_case wary \
    '<div class=chapter data-ok=yes>ok</div>' \
    '<div class=chapter data-ok=yes>ok</div>'

run_case wary \
    '<p title="unterminated>' \
    '&lt;p title="unterminated&gt;'

run_case wary \
    '<p>before</p><broken title="unterminated><p>after</p>' \
    '<p>before</p>&lt;broken title="unterminated&gt;&lt;p&gt;after&lt;/p&gt;'

echo "markup tests passed"
