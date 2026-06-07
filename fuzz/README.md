# Fuzzing

`epubscrub` includes fuzz-style checks for the EPUB ZIP reader and sanitizer pipeline.

Run a short standalone smoke fuzz with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
make fuzz-smoke
```

Increase the run count:

```sh
make fuzz-smoke FUZZ_RUNS=100000
```

If your compiler provides libFuzzer, build the coverage-guided target:

```sh
make fuzz-libfuzzer
mkdir -p fuzz/findings
./fuzz_epub -max_total_time=300 -artifact_prefix=fuzz/findings/ fuzz/corpus
```

Useful sanitizer options:

```sh
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 make fuzz-smoke
```

The fuzzers treat crashes, sanitizer findings, and timeouts as failures. A clean run does not prove every EPUB is safe; it raises confidence that malformed inputs are rejected without memory-safety failures.
