---
name: Bug report
about: Create a report to help us improve
title: ''
labels: bug
assignees: ''

---

**Describe the bug**
A clear and concise description of what the bug is.

**To Reproduce**
Steps to reproduce the behavior:

Exact command issued, with the input files attached, possibly minimized.

**Expected behavior**
A clear and concise description of what you expected to happen.

**Version (please complete the following information):**
 - Version (branch, etc.)
 - Theta version (if applicable)

**Additional context**
Add any other context about the problem here.

**Additional tips to pinpoint when the error was introduced:**

Run with ... and check if the error persists:

  - `--no-optimize`
  - `--memory=havoc --math=int`
    - note that these might break soundness. Check if the CFA is correct!
  - `--debug` flag to dump more data

Intermediate state dumps: check these files to pinpoint the problem.

  - `clang -S -emit-llvm input.c`
  - `gazer-cfa --run-pipeline <args>`
    - This (and the next) creates one file (.function.dot) per loop and per function.
  - `gazer-cfa ...` (this is for checking the backend only. Probably passing an optimized LLVM IR file is the best)
  - When running gazer-theta, check the output file
