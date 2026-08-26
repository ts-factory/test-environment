..
  SPDX-License-Identifier: Apache-2.0
  Copyright (C) 2026 OKTET Ltd.
  te-parent: te_ts

.. _te_scenario_md:

Test scenarios in markdown
==========================

A test scenario can be written as markdown before a line of C
exists. The ``scripts/scenario/scenario.py`` tool turns such a
description into a test stub with ``TEST_STEP()`` macros, and
later checks the implemented test against the description. The
markdown stays in the suite tree: it is the durable specification
of the test, readable by reviewers and by code generation tools
alike.

Files and naming
~~~~~~~~~~~~~~~~

Each package directory holds one ``package.md`` next to its
``package.xml``:

.. code-block:: none

    ts/usecases/
        package.xml
        package.md          the package scenario document
        set_mtu.md          optional per-test file
        set_mtu.c           the generated stub, then the test

The package name is the directory name; the test name is the
heading. Test ``set_mtu`` described in ``ts/usecases/package.md``
generates ``ts/usecases/set_mtu.c`` with
``TE_TEST_NAME "usecases/set_mtu"``.

A test is described either inline in ``package.md`` or in its own
file referenced from it; the two forms are interchangeable and
can be mixed in one document. Each package directory is
independent: the tool discovers ``package.md`` files by walking
the suite tree.

The dialect
~~~~~~~~~~~

A complete package document:

.. code-block:: markdown

    # usecases: Reliability in normal use

    Free prose is allowed anywhere at the top level.

    ## set_mtu: Set MTU of IUT

    Make sure that when we set the MTU, the port receives packets
    with size of no more than MTU, and does not receive others.

    Type: use case

    Parameters:

    - `mtu`: MTU on IUT
    - `ethdev_state`: the state of the device
      - `TEST_ETHDEV_CONFIGURED`: configured, not started
      - `TEST_ETHDEV_STARTED`

    Steps:

    1. Initialize EAL and configure the port.
    2. Set `mtu` on the port in `ethdev_state`.

       The driver may round the value down to the nearest
       supported MTU; read it back and use the effective value.

       > impl: use tapi_cfg_base_if_set_mtu(), not a raw cfg set.

    3. Transmit and check.
       - Send a packet of size `mtu`.
       - Check it is received.

    ## Referenced tests

    - [link_up_down](link_up_down.md)

The rules:

* ``# <package>: <summary>`` opens the document; ``<package>``
  must equal the directory name.
* ``## <test>: <summary>`` opens an inline test; the name matches
  ``[a-z0-9_]+``. Any other second-level heading is prose.
* A top-level list item ``- [<test>](<file>.md)`` delegates the
  test to a per-test file: the same syntax, with
  ``# <test>: <summary>`` as its first heading. A test name may
  appear only once per package.
* Inside a test, the first paragraph is the ``@objective``; a
  ``Type:`` paragraph sets the ``@type``; any other paragraph
  before ``Steps:`` becomes a ``@note``.
* ``Parameters:`` is followed by a list of
  ```` `name`: description ```` items; nested items list the
  values, optionally with a comment after a colon.
* ``Steps:`` is followed by the step list. Ordered and unordered
  markers are interchangeable at every depth.
* Both section markers may equally be written as a heading one
  level below the test heading, with or without the colon:
  ``### Steps`` in a package file, ``## Steps`` in a per-test
  file. The label and heading forms mean the same thing.

Steps, substeps and notes
~~~~~~~~~~~~~~~~~~~~~~~~~

The first paragraph of a step item is the step text. List depth
selects the macro: depth 1 is ``TEST_STEP()``, depth 2 is
``TEST_SUBSTEP()``, depth 3 and deeper use the
``TEST_STEP_PUSH()`` / ``TEST_STEP_NEXT()`` /
``TEST_STEP_POP()`` stack. Nesting deeper than three levels
produces a warning: such a step usually wants to be its own test.

Everything else inside a step item transfers into the stub as a
comment above the macro:

* a continuation paragraph is extra description: part of the
  specification that did not fit the one-liner;
* a blockquote (optionally tagged ``impl:``) is implementor
  advice: which TAPI to call, known pitfalls, where a reference
  implementation lives. It is prefixed ``IMPL:`` in the generated
  comment. Once the advice is consumed, the implementor is
  expected to delete the ``IMPL:`` text; the unprefixed part
  stays as documentation of the step.

Inline code spans
~~~~~~~~~~~~~~~~~

Backticked tokens in step text and parameter descriptions are
resolved when the stub is generated:

============================  ================================
Token                         Result
============================  ================================
a declared parameter name     ``@p name``
a listed value or ALL_CAPS    ``@c TOKEN``
anything else                 the bare token
============================  ================================

Text outside backticks is copied verbatim, so a raw ``@p`` or
``@c`` reference (for example to an environment interface such as
``iut_port`` that is not a test parameter) passes through
unchanged.

The tool
~~~~~~~~

Run from the test suite; the test root is ``-t`` (or ``-t/ts``
when that subdirectory exists, as in DPDK suites):

.. code-block:: shell

    # Write ts/usecases/set_mtu.c from the markdown
    ${TE_BASE}/scripts/scenario/scenario.py generate usecases/set_mtu

    # Compare every implemented test against its markdown;
    # non-zero exit on drift, so it can gate CI
    ${TE_BASE}/scripts/scenario/scenario.py check

    # What is described but not yet implemented
    ${TE_BASE}/scripts/scenario/scenario.py list --pending

The check compares the step texts, their order and their nesting
against the markdown; comments are never compared. A ``.c`` file
not described in any markdown is reported only with ``--strict``,
so a suite can adopt scenario documents incrementally.

Status
~~~~~~

The filesystem is the status: a test whose ``.c`` exists is
implemented, one without is pending. The markdown carries no
checkboxes or done-lists, and nothing needs updating when a test
is implemented - the drift check takes over from there.

The scenario of a built test
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Every ``TEST_STEP`` family macro also records its format string,
kind and source position into a ``te_scenario`` ELF section of the
test binary, so the step list of a built test is available without
running it and without its sources. Steps the framework logs
around every test, such as ``Test start`` and ``Test cleanup``,
are not recorded: the section holds the test's own scenario only.

.. code-block:: shell

    ${TE_BASE}/scripts/scenario/scenario.py steps ts/usecases/set_mtu

A test can also replay that scenario itself. When the
``TE_TEST_SCENARIO`` environment variable is set, a test logs every
recorded step through the normal logging path and exits
successfully without touching agents, RPC or the configurator. An
ordinary dispatcher run with the variable set therefore produces a
genuine log in which every iteration holds its scenario and
nothing else - suitable for the regular log processing tools and
importable like any other run:

.. code-block:: shell

    TE_TEST_SCENARIO=steps  ./run.sh --cfg=<testbed> ...
    TE_TEST_SCENARIO=params ./run.sh --cfg=<testbed> ...

``steps`` keeps ``@p name`` references as written; ``params``
substitutes them with the values of the iteration being logged.
Arguments of the C format specifiers are computed by the code that
replay skips, so they stay unexpanded.

The recording is the static step list of the binary: a step inside
a condition or a loop is recorded once and replayed once, whatever
the iteration would really do, and steps in helper functions appear
where they are defined, not where they are called. A scenario that
must be exact per iteration needs the test to hoist such conditions
into the step text, which is already the convention in the suites.
