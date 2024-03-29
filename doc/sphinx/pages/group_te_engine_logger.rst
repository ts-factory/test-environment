.. index:: pair: group; Logger
.. _doxid-group__te__engine__logger:

Logger
======

.. toctree::
	:hidden:

    group_sniffer.rst
    group_te_cmd_monitor.rst
	/generated/group_logger_api.rst
	/generated/group_te_log_stack.rst



.. _doxid-group__te__engine__logger_1te_engine_logger_introduction:

Introduction
~~~~~~~~~~~~

Logger provides an API for event logging to TE Subsystems, Test Agents and tests (if Test Agent runs on an operating system with shared address space, logging facilities may be provided to tested software as well).

Logger stores log messages in a binary file in raw format that can be later converted to XML, HTML and plain text as well as filtered.





.. _doxid-group__te__engine__logger_1te_engine_logger_operation:

Operation
~~~~~~~~~

.. image:: /static/image/ten_logger_context.png
	:alt: Logger context in TE

Logger passively accumulates log messages in raw log file when they are generated by :ref:`Test Engine <doxid-group__te__engine>` components by means of Logger API. From the other hand log messages generated in Test Agent context accumulated on Test Agent side by Logger Test Agent library until Logger asks for a bulk of messages (via ``RCFOP_GET_LOG`` RCF operation).

Each Test Agent is polled by Logger in regular time intervals in order to gather accumulated log messages (bulk of messages). Polling interval can be configured for each Test Agent in Logger configuration file.

Even though Logger exports two libraries - for Test Engine side (sources can be found under ${TE_BASE}/lib/loggerten directory) and Test Agent side (sources can be found under ${TE_BASE}/lib/loggerta directory), from users point of view there is no difference which library is in use because :ref:`API: Logger <doxid-group__logger__api>` hides all the differences.

Logger defines a number of log levels that gradate log messages by their importance. According to log levels Logger exports a number of macros to be used by any TE entity. These macros have printf-like arguments (format string and format arguments):

* :ref:`ERROR() <doxid-group__logger__api_1gab917aa4a73d839cc18faff31b013d680>` - errors and faults;

* :ref:`WARN() <doxid-group__logger__api_1gaf2e7f5ba954abedfcea33ca79e197e7d>` - warnings;

* :ref:`RING() <doxid-group__logger__api_1ga7b815b8f872c9036bba29ddfecd07368>` - significant events;

* :ref:`INFO() <doxid-group__logger__api_1gaa2da67464d48c3529acab6e54a97515b>` - informational;

* :ref:`VERB() <doxid-group__logger__api_1ga44b30c7ecb5a931abda27f5ede014926>` - verbose;

* :ref:`ENTRY() <doxid-group__logger__api_1ga02bfdb68ee815c98c614d7edf19e122e>` - function entry point;

* :ref:`EXIT() <doxid-group__logger__api_1ga51aaf0b1817f579662645cb6c2548aea>` - function exit.

Log level is a compile-time value – low log level leads to removing macro calls by the pre-processor. ERROR, WARN and RING are enabled by default.

Apart from C function API, Logger provides script-based mechanism for logging to use by :ref:`Dispatcher <doxid-group__te__engine__dispatcher>` and :ref:`Builder <doxid-group__te__engine__builder>`. This interface is necessary to support logging during initialization (when TEN components haven't been started yet):

* te_log_init - script to initialize logging facility;

* te_log_message - script to use for logging from scripts.





.. _doxid-group__te__engine__logger_1te_engine_logger_conf_file:

Configuration File
~~~~~~~~~~~~~~~~~~

:ref:`Logger <doxid-group__te__engine__logger>` has its own configuration file that can be used to specify:

* polling interval to use by Logger while gathering log messages from te_agents. It is possible to specify default interval that is applied to all te_agents as well as intervals for the particular Test Agent or for a goup of Agents of the same type;

* sniffer configuration settings.

For example the simplest :ref:`Logger <doxid-group__te__engine__logger>` configuration file would look like the following:

.. ref-code-block:: yaml

	loggerConf:
	# polling value unit is milli seconds.
		polling:
			_default: 100

If you need to specify polling intervals on per Test Agent basis or on per Agent type value you need to write something like:

.. ref-code-block:: yaml

	loggerConf:
	# polling value unit is milli seconds.
		polling:
	# specify polling interval on per Test Agent name basis
			agent:
				_agent: Agt_A
				_value: 200
	# specify polling interval on per Test Agent type basis
			type:
				_type: unix
				_value: 300
		_default: 100





.. _doxid-group__te__engine__logger_1te_engine_logger_log_msg:

Log messages
~~~~~~~~~~~~

Each log message is associated with:

* timestamp - time value when a log was registered in :ref:`Logger <doxid-group__te__engine__logger>`;

* level - the value that reflects importance of the message;

* entity name - identifies :ref:`Test Engine <doxid-group__te__engine>` process which logs the message (:ref:`Remote Control Facility (RCF) <doxid-group__te__engine__rcf>`, :ref:`Tester <doxid-group__te__engine__tester>`, test name, etc.) or TA where the message was generated;

* user name - identifies any logical part of the process like RCF API library, Configurator API library or agent configuration module. Each library must have its own logger user name;

Example of log message (in text format):

.. code-block:: none


	ERROR  Tester  Run Path  16:15:50 592 ms
	Test path requested by user not found.
	Path: foobar-ts/foo_package/mytest

All log messages accumulated by :ref:`Tester <doxid-group__te__engine__tester>` into tmp_raw_log file that is by default put under a directory from which :ref:`Dispatcher <doxid-group__te__engine__dispatcher>` script is called. The directory where to put tmp_raw_log file can be overwritten specifying --log-dir option to :ref:`Dispatcher <doxid-group__te__engine__dispatcher>`.

:ref:`Report Generator Tool <doxid-group__rgt>` should be used to convert raw logs into different formats. Conversion may be done in live or postponed modes.

Live mode is suitable to use when it is necessary to get log output in the text format on the fly. :ref:`Dispatcher <doxid-group__te__engine__dispatcher>` command-line option --live-log should be used to get logs on the fly.

In postponed mode raw log is first converted to XML format (structured, time ordered) and then XML file can be converted to HTML or text format log.

To ask :ref:`Dispatcher <doxid-group__te__engine__dispatcher>` prepare HTML version of the log you should pass --log-html=[output dir name] option in command line. Then :ref:`Dispatcher <doxid-group__te__engine__dispatcher>` generates HTML pages under specified [output dir name] directory.

|	:ref:`API: Logger<doxid-group__logger__api>`
|	:ref:`API: Logger messages stack<doxid-group__te__log__stack>`


