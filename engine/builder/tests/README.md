[SPDX-License-Identifier: Apache-2.0]::
[Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.]::

# Builder script tests

Tests for the Builder helper scripts. They do not build Test
Environment.

Each test is a self-contained shell script; run it directly:

```sh
./te_fetch_ext_repos.sh
./te_external_yml.sh
./te_meson_build.sh
```

Nothing runs them automatically: run them before changing anything
under `engine/builder`.

A test creates what it needs in a temporary directory and removes
it afterwards; `test_lib.sh` holds the shared part. The tests create
bare git repositories there and work against them, so they need no
network and touch nothing outside the temporary directory.

`te_meson_build.sh` sources the script instead of running it and
checks what a configuration with an external repository turns into
at the inputs of the build. It compiles nothing, so it needs neither
a build tree nor an agent, and it does not show that a library from
such a repository links.

Each script exits with a zero status when every check passes and
prints the failing checks otherwise.

The scripts source each other and the Builder, so check them with
`shellcheck -x`.
