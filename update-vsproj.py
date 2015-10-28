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

def main():
    source = [(matching_files('*.cpp'), 'ClCompile', 'Sources'),
              (matching_files('*.h'), 'ClInclude', 'Headers'),
              (matching_files('*.hpp'), 'ClInclude', 'Sources'),
              (matching_files('*.txt'), 'Text', 'Sources'),
              (matching_files('*.py'), 'None', 'Sources')]

    out = ''
    vcxproj = io.open(vcxproj_name, encoding='utf-8')
    for line in vcxproj:
        if 'src' in line and 'Include' in line:
            continue
        if 'wscript' in line:
            for src in source:
                for file in src[0]:
                    out += '    <{} Include="{}" />\n'.format(src[1], file)
        out += line

    vcxproj.close()
    file = open(vcxproj_name, 'wb')
    file.write(out.encode('utf-8'))

    out = ''
    filters = io.open(filters_name, encoding='utf-8')
    skip_count = 0
    for line in filters:
        if skip_count != 0:
            skip_count -= 1
            continue
        if 'src' in line and 'Include' in line:
            skip_count = 2
            continue
        if 'wscript' in line:
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
