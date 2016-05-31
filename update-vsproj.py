#!/usr/bin/env python
# coding: utf-8

"""
Tool for automagically updating the paths in visual studio project files.
Is part of the git commit hook, or can be just ran manually.
Takes no arguments, the paths are hardcoded here.
Tested to work with both python 2.7.9 and 3.4.3

(Python's default XML libraries are so horrible this just parses files as lines)
"""

from __future__ import unicode_literals
import fnmatch
import os
import io

vcxproj_name = 'teippi.vcxproj'
filters_name = 'Teippi.vcxproj.filters'

def matching_files(pattern):
    ret = []
    for root, _, filenames in os.walk('src'):
        ret += [os.path.join(root, x) for x in fnmatch.filter(filenames, pattern)]
    return sorted(ret)

def is_debug_only(file):
    if not file.endswith('.cpp'):
        return False
    if 'src\\console\\' in file:
        return True
    if 'scconsole.cpp' in file or 'test_game.cpp' in file:
        return True
    return False

def main():
    source = [(matching_files('*.cpp'), 'ClCompile', 'Source Files'),
              (matching_files('*.h'), 'ClInclude', 'Header Files'),
              (matching_files('*.hpp'), 'ClInclude', 'Source Files'),
              (matching_files('*.txt'), 'Text', 'Source Files'),
              (matching_files('*.py'), 'None', 'Source Files')]

    out = ''
    vcxproj = io.open(vcxproj_name, encoding='utf-8')
    written_sources = False
    skip_next_line = False
    current_group_label = 'something'
    for line in vcxproj:
        if '<ItemGroup' in line:
            if 'Label=' in line:
                current_group_label = 'something'
            else:
                current_group_label = None

        if skip_next_line:
            skip_next_line = False
            continue
        if 'src' in line and 'Include' in line:
            continue
        if '<ExcludedFromBuild' in line:
            skip_next_line = True
            continue
        if '</ItemGroup>' in line and not written_sources and current_group_label is None:
            written_sources = True
            for src in source:
                for file in src[0]:
                    if is_debug_only(file):
                        out += '    <{} Include="{}">\n'.format(src[1], file)
                        out += '      <ExcludedFromBuild Condition='
                        out += '"\'$(Configuration)|$(Platform)\'==\'Release|Win32\'">true'
                        out += '</ExcludedFromBuild>\n'
                        out += '    </{}>\n'.format(src[1])
                    else:
                        out += '    <{} Include="{}" />\n'.format(src[1], file)
        out += line

    vcxproj.close()
    file = open(vcxproj_name, 'wb')
    file.write(out.encode('utf-8'))

    out = ''
    filters = io.open(filters_name, encoding='utf-8')
    skip_count = 0
    group_num = 0
    for line in filters:
        if skip_count != 0:
            skip_count -= 1
            continue
        if 'src' in line and 'Include' in line:
            skip_count = 2
            continue
        if '</ItemGroup>' in line:
            group_num += 1
        if '</ItemGroup>' in line and group_num == 2:
            for src in source:
                for file in src[0]:
                    out += '    <{} Include="{}">\n'.format(src[1], file)
                    dirname = os.path.dirname(file)
                    if dirname == 'src':
                        out += '      <Filter>{}</Filter>\n'.format(src[2])
                    else:
                        out += '      <Filter>{}\\{}</Filter>\n'.format(src[2], dirname[4:])
                    out += '    </{}>\n'.format(src[1])
        out += line

    filters.close()
    file = open(filters_name, 'wb')
    file.write(out.encode('utf-8'))

if __name__ == '__main__':
    main()
