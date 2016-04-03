#!/usr/bin/env python
# coding: utf-8
import sys
import re

indent = '    '

c_nuottei_types = [
    ('uint32_t', 'dword'), ('uint16_t', 'word'), ('uint8_t', 'byte'), ('char', 'char'),
    ('void', 'void'), ('int', 'int'), ('x32', 'x32'), ('y32', 'y32')
]

def ToCType(name):
    for pair in c_nuottei_types:
        name = name.replace(pair[1], pair[0])
    return name

def IsType(name):
    return name in [pair[1] for pair in c_nuottei_types]

class Func:
    def __init__(this, line, module):
        match = re.match('(?P<addr>[0-9A-F]{8})\s=\s(?P<return_attr_name>.*)\(\)($|(, (?P<args>.*)))', line)
        if match:
            this.module = module
            this.addr = match.group('addr')
            type_f = '()|(?P<ret_type>(.*?\S))' # Nothing or anything not ending in space, lazy, as name is greedy
            attr_f = '()|(\s\((?P<attr>.+)\))' # Nothing or everything inside braces
            ran_m = re.match('({ret_type})({attr})\s?(?P<name>[^*()\s]*)$'.format(ret_type=type_f, attr=attr_f),
                match.group('return_attr_name'))

            if ran_m.group('ret_type'):
                this.ret = ran_m.group('ret_type')
            else:
                this.ret = 'void'
            this.attr = []
            if ran_m.group('attr'):
                this.attr = ran_m.group('attr').split()
            this.name = ran_m.group('name')
            this.args = []
            if match.group('args'):
                args = match.group('args').split(', ')
                for arg in args:
                    arg_match = re.match('arg (\d+) (.*)', arg)
                    if arg_match:
                        stack_arg = True
                        reg = int(arg_match.group(1))
                    else:
                        stack_arg = False
                        arg_match = re.match('(\S+) (.*)', arg)
                        reg = arg_match.group(1)
                    rest = arg_match.group(2)
                    rest.strip()
                    if ' ' in rest:
                        rest_match = re.match('(?P<type>.*?\S)\s?(?P<name>[^*()\s]*)$', rest)
                        arg_type = rest_match.group('type')
                        arg_name = rest_match.group('name')
                        if arg_name == '':
                            arg_name = None
                        else:
                            arg_name = 'arg_' + arg_name
                    else:
                        if IsType(rest):
                            arg_type = rest
                            arg_name = None
                        else:
                            arg_name = 'arg_' + rest
                            arg_type = None
                    this.args += [(stack_arg, reg, arg_type, arg_name)]

        else:
            raise ValueError('Not a func: "{}"'.format(line))

def IsSection(line):
    match = re.match('### (?P<name>\S+)( \((?P<base>[\dA-F]+)\))?', line)
    if match:
        if match.group('base'):
            return {'base': int(match.group('base'), 16), 'name': match.group('name')}
        return True
    return False

def CArgs(args):
    ret = ''
    num = 1
    for arg in args:
        if ret != '':
            ret += ', '
        type = ToCType(arg[2] or 'int')
        name = arg[3] or 'a{}'.format(num)
        ret = ret + type + ' ' + name
        num += 1
    return ret

def ToCCompatName(name):
    return name.replace('.', '_').replace('-', '_')

def ToGccConstraint(name):
    if name == 'esi':
        return 'eS'
    elif name == 'edi':
        return 'eD'
    return name[:2]

def GenerateGccAsm(func, clang):
    ret = ''
    ret += 'inline {type} {name}({args})\n{{\n'.format(type=ToCType(func.ret), name=func.name, args=CArgs(func.args))
    stack_size = 0
    for arg in func.args:
        if arg[0]:
            stack_size += 4

    inputs = ''
    if func.ret != 'void':
        ret += indent + 'int ret;\n'
    if stack_size:
        ret += indent + 'register uintptr_t *stack asm("esp");\n'
        # Clang would undefined behaviour without "assigning" something to the stack
        ret += indent + 'asm("" : "=r"(stack));\n'
        num = 1
        for arg in func.args:
            name = arg[3] or 'a{}'.format(num)
            if arg[0]:
                ret += indent + 'stack[{pos}] = (uintptr_t){name};\n'.format(pos=-(stack_size // 4) + arg[1] - 1, name=name)
            num += 1

    num = 1
    freeregs = ['eax', 'ecx', 'edx', 'ebx', 'esi', 'edi']
    for arg in func.args:
        name = arg[3] or 'a{}'.format(num)
        if not arg[0]:
            ret += '{ind}register uintptr_t reg_{name} asm("{reg}");\n{ind}reg_{name} = (uintptr_t){name};\n'.format(ind=indent, name=name, reg=arg[1])
            inputs += '"{}"(reg_{name}), '.format(ToGccConstraint(arg[1]), name=name)
            if arg[1] in freeregs:
                freeregs.remove(arg[1])
        num += 1

    ret += indent + 'register uintptr_t _target asm("{reg}");\n'.format(reg=freeregs[0])
    if func.module:
        ret += indent + '_target = 0x{addr} + {name}::base_diff;\n'.format(addr=func.addr, name=ToCCompatName(func.module['name']))
    else:
        ret += indent + '_target = 0x{addr};\n'.format(addr=func.addr)

    ret += indent + 'asm("'
    num = 1
    input_count = 0
    if stack_size:
        ret += 'sub ${}, %%esp\\n\\t"\n'.format(stack_size)
        ret += indent + '    "'

    ret += 'call *%%{reg}'.format(reg=freeregs[0])
    if 'cdecl' in func.attr:
        ret += '\\n\\t"\n' + indent + '    "add ${}, %%esp'.format(stack_size)

    ret += '"\n{ind}    '.format(ind=indent)
    outputs = ''
    clobber = '"memory"'
    #if freeregs[0] == 'ebx' or freeregs[0] == 'esi' or freeregs[0] == 'edi':
        #clobber += ', "{}"'.format(freeregs[0])
    
    inputs += '"{}"(_target) '.format(freeregs[0])
    # Gcc doesn't accept conflicting clobber/ret, but clang may place something between
    # the call statement and empty clobber statement. Maybe gcc can do it as well?
    if clang:
        if func.ret != 'void':
            outputs += '"=ea"(ret)'
        else:
            clobber += ', "eax"'
        clobber += ', "ecx", "edx"'

    ret += ': {}: {}: {});\n'.format(outputs, inputs, clobber)

    if not clang:
        if func.ret != 'void':
            ret += indent + 'asm volatile("" : "=ea"(ret): : "ecx", "edx");\n'
        else:
            ret += indent + 'asm volatile("" : : : "eax", "ecx", "edx");\n'
    if func.ret != 'void':
        ret += '{ind}return ({type})ret;\n'.format(ind=indent, type=ToCType(func.ret))

    ret += '}\n'
    return ret

def GenerateMsvcAsm(func):
    ret = ''
    ret += 'inline {type} {name}({args})\n{{\n'.format(
            type=ToCType(func.ret), name=func.name, args=CArgs(func.args))

    stack_size = 0
    for arg in func.args:
        if arg[0]:
            stack_size += 4

    ret += indent + '__asm {\n'

    freeregs = ['eax', 'ecx', 'edx', 'ebx', 'esi', 'edi']

    num = 1
    # Push all stack args first
    stack_arg_order = []
    for arg in func.args:
        name = arg[3] or 'a{}'.format(num)
        if arg[0]:
            stack_arg_order += [(arg[1], name)]
        num += 1
    stack_arg_order.sort(reverse=True)
    for arg in stack_arg_order:
        ret += '{ind}push {name};\n'.format(ind=indent + indent, name=arg[1])
    num = 1
    # Then order register args
    for arg in func.args:
        name = arg[3] or 'a{}'.format(num)
        if not arg[0]:
            ret += '{ind}mov {reg}, {name}\n'.format(ind=indent + indent, name=name, reg=arg[1])
            if arg[1] in freeregs:
                freeregs.remove(arg[1])
        num += 1

    # And use one free register as the call target
    # Calling an immediate doesn't play nice with dll rebasing
    ret += '{ind}mov {reg}, 0x{addr}\n'.format(ind=indent + indent, reg=freeregs[0], addr=func.addr)
    if func.module:
        name = ToCCompatName(func.module['name'])
        ret += '{ind}add {reg}, {name}::base_diff\n'.format(ind=indent + indent, reg=freeregs[0], name=name)
    ret += '{ind}call {reg}\n'.format(ind=indent + indent, reg=freeregs[0])

    if 'cdecl' in func.attr:
        ret += '{ind}add esp, {size}\n'.format(ind=indent + indent, size=stack_size)

    ret += indent + '}\n'

    ret += '}\n'
    return ret

def GenerateFuncs(nuottei, msvc_asm, clang_asm):
    filu = open(nuottei)
    funcs = []
    out = b''
    module = None
    for line in filu:
        line = line.strip()
        if line == '':
            pass
        else:
            new_mod = IsSection(line)
            if new_mod == False:
                funcs += [Func(line, module)]
            elif new_mod != True:
                module = new_mod
    for func in funcs:
        if msvc_asm:
            out += bytes(GenerateMsvcAsm(func), 'utf-8')
        else:
            out += bytes(GenerateGccAsm(func, clang_asm), 'utf-8')
        out += b'\n'
    return out
    
def main():
    try:
        nuottei = sys.argv[1]
        output = sys.argv[2]
    except IndexError:
        print('functonuot.py <nuottei.txt> <funcs.autogen>')
        return

    msvc_asm = '--msvc' in sys.argv
    clang_asm = '--clang' in sys.argv
    assert(not (msvc_asm and clang_asm))
    data = GenerateFuncs(nuottei, msvc_asm, clang_asm)
    out = open(output, 'wb') # Windows line endingit voi painua hiitee
    out.write(data)

if __name__ == "__main__":
    main()

