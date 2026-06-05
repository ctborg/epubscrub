# Security Policy

`epubscrub` is a sanitizer, not a malware-detection guarantee.

## Reporting Security Issues

Please do not disclose exploitable sanitizer bypasses publicly before maintainers have had a chance to respond.

Until a dedicated security contact exists, open a GitHub issue with minimal public detail and ask for a private contact path.

## Scope

Security-sensitive issues include:

- ZIP parsing bugs
- Path traversal bypasses
- Sanitizer bypasses for active EPUB content
- Crashes on malformed EPUB input
- Output EPUBs that preserve files a policy says should be removed

## Safe Testing

Use synthetic EPUB fixtures for reports. Do not upload copyrighted books, live malware, or payloads that execute outside a controlled environment.
