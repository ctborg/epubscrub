# epubscrub

![epubscrub](epub.jpg)

`epubscrub` is a small open source C utility that sanitizes EPUB files by removing common active-content and archive-based exploit vectors.

It is a sanitizer, not a promise that a file is malware-free.

## Features

- Safe EPUB ZIP inspection and rewriting
- `calm`, `wary`, and `paranoid` policy modes
- Text and JSON reports
- JavaScript, event-handler, unsafe URL, suspicious file, and path-traversal cleanup
- Fuzz-style smoke testing with ASan/UBSan
- Manual page with `make install`
- macOS and Linux CI

## Build

```sh
make
```

Requires a C11 compiler and `zlib`.

## Install

```sh
make install
```

By default, the Makefile installs under `/opt/homebrew` when that directory exists, otherwise `/usr/local`.
You can override this:

```sh
make install PREFIX="$HOME/.local"
```

## Usage

```sh
./epubscrub input.epub -o clean.epub
./epubscrub --check input.epub
./epubscrub input.epub --report report.txt
./epubscrub --check --paranoid --report-format json input.epub
./epubscrub --version
```

`--check` performs the same scan and in-memory sanitization pass without writing an output file. `--report` without `-o` also runs as report-only check. If a file would be removed, the report includes the reason, for example:

```text
remove file: OEBPS/evil.js (reason: JavaScript is active content and can execute in EPUB readers)
```

## Policy Modes

`epubscrub` defaults to `wary`.

- `--calm` or `--policy calm`: removes clear active/script/executable payloads while leaving more normal EPUB resources alone
- `--wary` or `--policy wary`: default practical baseline; removes active content, unknown types, external resource loads, and unsafe CSS URLs
- `--paranoid` or `--policy paranoid`: minimizes attack surface; also removes SVG, fonts, media, and external text links

Reports can be text or JSON:

```sh
./epubscrub --check --policy paranoid --report-format json book.epub
```

## Development

```sh
make clean
make
make test
make fuzz-smoke
mandoc -Tlint man/epubscrub.1
```

For longer fuzz-style runs:

```sh
make fuzz-smoke FUZZ_RUNS=100000
```

See [fuzz/README.md](fuzz/README.md) for the optional libFuzzer target.

Exit codes:

- `0`: input was already clean
- `1`: EPUB was sanitized with changes
- `2`: EPUB was rejected as unsafe or malformed
- `3`: read/write/parse error
- `4`: internal error

## Current Policy

The default `wary` policy removes:

- EPUB archives over the configured size limit
- JavaScript and executable-like files
- Unknown binary/archive file types
- ZIP entries with path traversal or unsafe names
- `<script>`, `<iframe>`, `<object>`, `<embed>`, and form controls in XHTML/SVG
- Event handler attributes such as `onclick` and `onload`
- External resource loads in `<img>`, `<link>`, and `<iframe>` tags
- `javascript:`, `vbscript:`, and unsafe `data:` URLs
- CSS `@import` rules and remote/script/data `url(...)` values

The default `wary` policy preserves:

- Local CSS stylesheets
- SVG files after script and unsafe-reference sanitization
- Internal navigation links such as `#anchor` and `../chapter2.xhtml`
- Font files: `.ttf`, `.otf`, `.woff`, `.woff2`
- Standard metadata in `content.opf`, `toc.ncx`, and `nav.xhtml` after sanitization

It also rewrites OPF manifest entries that point to files removed by the sanitizer. JavaScript files are removed outright, including obfuscated/minified JS and JS that could make network requests.

`epubscrub` cannot verify that the reader app itself is maintained. Use a current EPUB reader from a trusted vendor when opening files from unknown sources.

## License

MIT. See [LICENSE](LICENSE).
