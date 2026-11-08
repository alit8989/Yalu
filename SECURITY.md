# Security Policy

## Supported Versions

Security updates and critical bug fixes are applied to the active development version of Yalu.

| Version | Supported          |
| :------ | :----------------- |
| 0.3.x   | :white_check_mark: |
| < 0.3.0 | :x:                |

---

## Reporting a Vulnerability

The Yalu development team takes the security of the interpreter and runtime seriously. If you discover a vulnerability or security flaw (such as an exploitable buffer overflow, denial of service, memory corruption, or unsafe recursion bypass), please report it responsibly.

### How to Report

1. **GitHub Security Advisory**: If viewing this project on GitHub, please navigate to the **Security** tab and click **Report a vulnerability** to submit a private draft advisory.
2. **Repository Issues**: If private advisories are unavailable, open a regular issue requesting a private security coordination channel before sharing detailed proof-of-concept exploits.

### What to Include in Your Report

To help us triage and resolve the issue quickly, please provide:
- A clear description of the vulnerability and its potential impact.
- The exact Yalu version or commit hash where the bug was observed.
- Step-by-step reproduction instructions or a minimal `.yl` script demonstrating the problem.
- Any mitigation steps or proposed patches if available.

### Response Timeline

- **Initial Acknowledgment**: Maintainers aim to acknowledge reports within 3 business days.
- **Triage & Fix**: A fix will be developed in a private branch, verified against existing test suites, and published with a release note and credit to the reporter.

