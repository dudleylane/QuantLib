# Security policy

## Reporting a vulnerability

Report security problems privately, through GitHub's private vulnerability reporting. Use **Report a
vulnerability** on the repository's [Security tab](https://github.com/dudleylane/QuantLib/security), or go
straight to [the report form](https://github.com/dudleylane/QuantLib/security/advisories/new). Don't open a
public issue, pull request or discussion for one: that would disclose the problem before a fix exists.

Report these privately:

- Anything that data a calling application may not control can trigger. That includes a crash, a hang,
  unbounded memory or CPU use, or a memory-safety defect such as an out-of-bounds access, a use after free or
  other undefined behaviour. Such data includes market-data files read by `CsvQuoteLoader` or
  `JsonQuoteLoader`, and pricing, curve or model inputs that an application passes through from its own
  users.
- Anything that would let a pull request, or any other outside party, run code on this project's
  self-hosted CI runner.

If you're not sure whether something qualifies, report it privately anyway.

A useful report gives:

- the class or function involved;
- the configuration: the `QL_*` options, the compiler and the C++ standard;
- the commit;
- the smallest input that reproduces the problem.

A numerical defect with no security impact, such as a wrong price or a failed calibration, is an ordinary
bug. Open it as an [issue](https://github.com/dudleylane/QuantLib/issues).

## What happens next

The report becomes a draft security advisory that only you and the maintainers can see. The fix is worked
out there, and the advisory is published together with the fix.

## Supported versions

Fixes go to `master`. This fork has no releases and no maintenance branches. If the problem is also present
in upstream QuantLib ([lballabio/QuantLib](https://github.com/lballabio/QuantLib)), say so in the report.
