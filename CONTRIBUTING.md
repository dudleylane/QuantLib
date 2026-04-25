
# Contributing to QuantLib

> **This is the `dudleylane/QuantLib` fork.**  Choose your destination
> before you start:
>
> - **Fixes or features for the fork** (anything in `FORK_CHANGES.md`,
>   the dual-license arrangement, the C++23 / GCC ≥ 15.2.1 toolchain,
>   or the new modules under `ql/risk/`, `ql/marketdata/`,
>   `ql/pricingengines/autocallable/`) — open a PR or issue at
>   <https://github.com/dudleylane/QuantLib>.  Please read `CLAUDE.md`
>   first for the Decision-Making Policy and pre-commit checklist.
> - **Fixes for upstream QuantLib** (the original BSD-licensed library)
>   — open against <https://github.com/lballabio/QuantLib> using the
>   instructions below; the fix will be brought across the divergence
>   point separately.
> - **License note** — new files added to this fork are AGPL-3.0+;
>   upstream files retain their BSD-3-Clause headers.  See
>   `README.md`'s License section for the dual-license rules before
>   adding new files.

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
