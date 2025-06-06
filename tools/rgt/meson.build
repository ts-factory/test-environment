# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2018-2022 OKTET Labs Ltd. All rights reserved.

# Common variable for rgt
package = 'rgt'
version = '0.4'
te_copyright = '# Copyright (C) 2004-2025 OKTET Labs Ltd. All rights reserved.'

# Common includes

inc = include_directories(
    'lib/tmpls',
    '.',
)
inc = [inc, te_include]

# Configure config.h and add define
c_args = ['-DHAVE_CONFIG_H']

cdata = configuration_data()
cdata.set_quoted('PACKAGE', package)
cdata.set_quoted('VERSION', version)
cdata.set('WITH_LIBXML', 1)

configure_file(output: 'config.h', configuration: cdata)

# Add dependencies

dep_glib = dependency('glib-2.0', version: '>= 2.4.8', required: false)
required_deps += 'glib-2.0'
if not dep_glib.found()
    missed_deps += 'glib-2.0'
endif

# Check for obstack feature

obstack_code = '''
    #include <stdlib.h>
    #include <obstack.h>
    int main ()
    {
        struct obstack mem;
        #define obstack_chunk_alloc malloc
        #define obstack_chunk_free free
        obstack_init (&mem);
        obstack_free (&mem, 0);
        return 0;
    }
'''

does_link = cc.links(
    obstack_code,
    name: 'obstack link check',
)

if not does_link
    error('Obstack feature is required to build RGT')
endif

# Add large logs support if required

if get_option('large-logs')
    c_args += ['-D_LARGEFILE64_SOURCE', '-D_FILE_OFFSET_BITS=64']
endif

# RGT uses many GNU extensions:
#  - obstack_printf
#  - program_invocation_short_name
# Force it here to be able to build the rest w/o _GNU_SOURCE.
c_args += '-D_GNU_SOURCE'

# Check libxml members

_xmlSAXHandler_members = [
    '_private',
    'endElementNs',
    'serror',
    'startElementNs',
]

required_deps += 'libxml-2.0'
foreach member: _xmlSAXHandler_members
    if not cc.has_member('struct _xmlSAXHandler', member,
            prefix: '#include <libxml/parser.h>', dependencies: [dep_libxml2])
        warning('Member ' + member + ' of struct _xmlSAXHandler not found')
        missed_deps += 'libxml-2.0'
    endif
endforeach

# Find program that generates headers with templates

header_gen_script = find_program('gen_tmpls_h.sh', required: true)

install_data(
    [
        'rgt-proc-raw-log',
        'rgt-log-get-item',
        'rgt-bublik-json',
        'rgt-bublik-json-legacy',
        'rgt-tester-mi-version',
    ],
    install_dir: get_option('bindir'),
)

# Process subdirectories

subdir('lib')
subdir('rgt-core')
subdir('rgt-dump')
subdir('rgt-idx')
subdir('rgt-log-bundle')
subdir('rgt-pdml2xml')
subdir('rgt-xml-merge')
subdir('rgt-format')
