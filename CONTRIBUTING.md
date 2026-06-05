# Contributing

Thanks for helping make `epubscrub` safer and more useful.

## Development

Build and test locally:

```sh
make clean
make
make test
mandoc -Tlint man/epubscrub.1
```

The project currently depends on a C11 compiler and `zlib`.

## Guidelines

- Keep sanitizer behavior conservative and document security tradeoffs.
- Add regression tests for every sanitizer rule change.
- Avoid claiming an EPUB is malware-free; this tool reduces common exploit surfaces.
- Keep policy behavior clear across `calm`, `wary`, and `paranoid`.
- Preserve unrelated EPUB content unless a policy explicitly removes it.

## Reports

Bug reports are most useful when they include:

- The command used
- The selected policy
- The report output
- A minimal EPUB fixture when possible

Do not attach copyrighted books publicly. Create a small synthetic EPUB fixture instead.
