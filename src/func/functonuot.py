#!/usr/bin/env python
# coding: utf-8
import sys
import re

c_nuottei_types = [
    ('uint32_t', 'dword'), ('uint16_t', 'word'), ('uint8_t', 'byte'), ('char', 'char'),
    ('void', 'void'), ('Void', 'void') # Lowercase void ekana joten se on preferred
]
def CTypeToNuottei(val):
    for pair in c_nuottei_types:
        val = val.replace(pair[0], pair[1])
    return val

def RetToNuottei(val):
    match = re.match('Cdecl<(.+)>', val)
    if match:
        return '{type} (cdecl)'.format(type=CTypeToNuottei(match.group(1)))
    else:
        return CTypeToNuottei(val)

def ArgsToNuottei(args):
    ret = ''
    argpos = 1
    for arg in args:
        if ret != '':
            ret += ', '
        match = re.match('stack<([0-9]+), (.+)>', arg)
        if match:
            ret += 'arg {num} {type}'.format(num=match.group(1) + 1, type=CTypeToNuottei(match.group(2)))
        else:
            match = re.match('([a-z]{3})<(.+)>', arg)
            if match:
                ret += '{reg} {type}'.format(reg=match.group(1), type=CTypeToNuottei(match.group(2)))
            else:
                ret += 'arg {num} {type}'.format(num=argpos, type=CTypeToNuottei(arg))
                argpos += 1
    return ret

def ParseArgs(args):
    mid = [s.strip() for s in args.split(',')]
    ret = []
    i = 0
    while i < len(mid):
        if 'stack' in mid[i]:
            ret += [mid[i] + ', ' + mid[i + 1]]
            i += 2
        else:
            ret += [mid[i]]
            i += 1
    return ret

def ParseFuncs(funcs):
    filu = open(funcs)
    funcs = []
    out = b''
    for line in filu:
        line = line.strip()
        match = re.match('const func<([a-zA-Z\s,<>_:*0-9]+)> ([a-zA-Z_][a-zA-Z_0-9]*) = 0x([0-9a-fA-F]+);', line)
        if match:
            args = match.group(1)
            name = match.group(2)
            address = match.group(3)
            funcs += [(ParseArgs(args), name, address)]
        elif len(funcs) > 0 and funcs[-1] != None:
            funcs += [None]
    
    for func in funcs:
        if func:
            ret = RetToNuottei(func[0][0])
            if ret[-1] == '*':
                sp = ''
            else:
                sp =  ' '

            line = '{} = {ret}{space}{name}(), {args}'.format(func[2], space=sp, name=func[1], ret=ret, args=ArgsToNuottei(func[0][1:]))
            if line[-2:] == ', ':
                line = line[:-2]
            out += bytes(line, 'utf-8')
        out += b'\n'
    return out
            
    
def main():
    try:
        funcs = sys.argv[1]
        nuottei = sys.argv[2]
    except IndexError:
        print('functonuot.py <offsets.h> <nuottei.txt>')
        return

    data = ParseFuncs(funcs)
    out = open(nuottei, 'wb') # Windows line endingit voi painua hiitee
    out.write(data)

if __name__ == "__main__":
    main()

