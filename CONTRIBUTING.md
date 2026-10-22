
# Contributing to QuantLib

> **This is the `dudleylane/QuantLib` fork.**  Choose your destination
> before you start:
>
> - **Fixes or features for the fork** (anything in `FORK_CHANGES.md`,
>   the dual-license arrangement, the C++23 / GCC ≥ 15.2.1 toolchain,
>   or the new modules under `ql/risk/`, `ql/marketdata/`,
>   `ql/pricingengines/autocallable/`) — open a PR or issue at
>   <https://github.com/dudleylane/QuantLib>.  Please work through the
>   pre-commit checklist below before pushing.
> - **Fixes for upstream QuantLib** (the original BSD-licensed library)
>   — open against <https://github.com/lballabio/QuantLib> using the
>   instructions below.  This fork does not track upstream, so a fix
>   merged there does not reach the fork; if the bug affects the fork
>   too, open an issue here as well.
> - **License note** — new files added to this fork are AGPL-3.0+;
>   upstream files retain their BSD-3-Clause headers.  See
>   `README.md`'s License section for the dual-license rules before
>   adding new files.

---

## Pre-commit checklist

Before pushing a change to code, build files, workflows or scripts:

1. **Build and test.**  Build with GCC ≥ 15.2.1 (see *Build, test, run*
   in `README.md`).  Run the suites the change touches, then the whole
   suite.
2. **Format** with `clang-format-22` (see *Formatting* below).
3. **Keep both build systems in step.**  A file added, removed or
   renamed goes in `ql/CMakeLists.txt` (or `test-suite/CMakeLists.txt`)
   *and* in the directory's `Makefile.am`.  A header also goes in that
   directory's tracked `all.hpp`, which the autotools build regenerates
   from `Makefile.am`; that file is how `<ql/quantlib.hpp>` reaches the
   header.  This runs the same check as CI's `filelists` gate:

   ```bash
   ./autogen.sh && ./configure --disable-json-marketdata && ./tools/check_filelists.sh
   ```

4. **Headers compile on their own**, as CI's `headers` job checks:
   `echo ./ql/<dir>/<file>.hpp | python3 tools/check_header.py`.
5. **Configuration-dependent code** (dates, lazy objects and
   observers, coupons, `Settings`) must also pass under the
   `linux-ci-build-with-nonstandard-options` CMake preset.
6. **Licence and register.**  New files carry the AGPL-3.0+ header that
   the fork's other new files use (for example
   `ql/risk/curvebucketer.hpp`).  Upstream files keep their BSD-3-Clause
   headers untouched.  Record a fork-visible change in `FORK_CHANGES.md`.

Commit subjects follow the conventional-commit style of the log (`fix:`,
`test:`, `ci:`, `docs(README):` …).  The body says what was wrong and
where, how it was found, why this fix was chosen over the alternatives,
and the evidence that it works.

## Formatting

Project style is BSD/Allman braces, 120-column limit, 4-space indent
(see `.clang-format`).  CI enforces it on every push and PR.  The
`clang-format` job in `.github/workflows/ci.yml` runs
`clang-format-22 --dry-run -Werror`, so a PR with formatting drift
fails before review.

- **Pinned to clang-format 22.x.**  CentOS Stream 10 ships
  `clang-tools-extra` at LLVM 22.x natively, so `dnf install -y
  clang-tools-extra` gives the right binary; do **not** use
  `apt.llvm.org`, a self-built clang-format, or a different major
  version — `BasedOnStyle: LLVM` defaults shift across them and
  patch versions matter (we hit a 22.1.1 ↔ 22.1.3 skew when the
  gate first ran).  Call it by its versioned name, as the gate does.
  An unversioned `clang-format` that comes first on your path (pip
  installs 23.x) reports violations that 22.x does not.  To reformat
  locally before pushing:

  ```bash
  find ql test-suite Examples fuzz-test-suite \
      -type f \( -name '*.hpp' -o -name '*.cpp' \) -print0 \
    | xargs -0 clang-format-22 -i
  ```

- **One-time `git blame` setup.**  The project-wide reformat sits
  in commit `710698abd` and is recorded in `.git-blame-ignore-revs`.
  Per-clone, run this once so `git blame` skips the mechanical
  reformat:

  ```bash
  git config blame.ignoreRevsFile .git-blame-ignore-revs
  ```

- **clang-format oscillation escape hatch.**  If a future
  contributor hits a block that clang-format can't format stably
  (one such block exists in `ql/experimental/math/latentmodel.hpp`),
  fence it with `// clang-format off` / `// clang-format on` rather
  than reformatting around it.

---

## Upstream contribution flow

Thanks for considering a contribution!  We're looking forward to it.

The preferred way to contribute is through a pull request on GitHub.
This gives us some convenient tooling to look at your changes and
provide feedback; also, opening a pull request triggers automated
building and testing of your code and often gives you feedback before
a human has a chance to look at it (the time we can give to the
project is, unfortunately, limited).

So, in short: get a GitHub account if you don't have it already and
clone the repository at <https://github.com/lballabio/QuantLib> with
the "Fork" button in the top right corner of the page.  Check out your
clone to your machine, code away, push your changes to your clone and
submit a pull request: links to more detailed instructions are at the
end of this file.

A note: a pull request will show any new changes committed and pushed
to the corresponding branch.  For this reason, we strongly advise you
to use a feature branch for your changes, instead of your `master`
branch.  This gives you the freedom to add unrelated changes to your
master, and also gives the maintainers the freedom to push further
changes to the branch.

It's likely that we won't merge your code right away, and we'll ask
for some changes instead. Don't be discouraged! That's normal; the
library is complex, and thus it might take some time to become
familiar with it and to use it in an idiomatic way.

Again, thanks &mdash; and welcome!  We're looking forward to your contributions.


#### Useful links

Instructions for forking a cloning a repository are at
<https://help.github.com/articles/fork-a-repo>.

More detailed instructions for creating pull requests are at
<https://help.github.com/articles/using-pull-requests>.

Finally, a basic guide to GitHub is at
<https://guides.github.com/activities/hello-world/>.
GitHub also provides interactive learning at <https://lab.github.com/>.
