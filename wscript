#!/usr/bin/env python
# coding: utf-8

import os

def options(opt):
    opt.load('compiler_cxx')
    opt.add_option('--debug', action='store_true', help='"Debug" build', dest='debug')
    opt.add_option('--nodebug', action='store_true', help='Release build (Default)', dest='nodebug')
    opt.add_option('--optimize', action='store_true', help='Optimize always', dest='optimize')
    opt.add_option('--static', action='store_true', help='Enable static linkage (Default)', dest='static')
    opt.add_option('--nostatic', action='store_true', help='Disable static linkage', dest='nostatic')
    opt.add_option('--console', action='store_true', help='Console support (Requires freetype2)', dest='console')
    opt.add_option('--noconsole', action='store_true', help='Disable console (default)', dest='noconsole')
    opt.add_option('--perftest', action='store_true', help='Performance logging (Slow)', dest='perf_test')
    opt.add_option('--synctest', action='store_true', help='Sync logging (Huge logfiles)', dest='sync_test')
    opt.add_option('--freetype-include', action='store', help='Freetype include directory', dest='ft_inc')
    opt.add_option('--freetype-lib', action='store', help='Freetype library directory', dest='ft_lib')
    opt.add_option('--freetype-link', action='store', help='Freetype link flags', dest='ft_link')
    opt.add_option('--forced-seed', action='store', help='Force sc use always same rng seed', dest='forced_seed')

def configure(conf):
    import waflib.Tools.compiler_cxx as compiler_cxx
    import waflib.Errors

    conf.env.MSVC_TARGETS = 'x86'

    compiler_cxx.configure(conf)

    if conf.options.console:
        if conf.options.ft_inc and conf.options.ft_lib:
            conf.env.ft_inc = conf.options.ft_inc
            conf.env.ft_lib = conf.options.ft_lib
            conf.env.ft_link = conf.options.ft_link
        else:
            try:
                conf.check_cfg(package='freetype2', args='--cflags --libs', uselib_store='freetype')
                conf.env.freetype_pkg = True
            except waflib.Errors.ConfigurationError:
                conf.fatal('Freetype not found, please pass --freetype-include and --freetype-lib manually')

    conf.env.debug = conf.options.debug and not conf.options.nodebug
    conf.env.optimize = conf.options.optimize
    conf.env.static = conf.options.static or not conf.options.nostatic
    conf.env.console = conf.options.console and not conf.options.noconsole
    conf.env.perf_test = conf.options.perf_test
    conf.env.sync_test = conf.options.sync_test
    conf.env.forced_seed = conf.options.forced_seed

    conf.env.cshlib_PATTERN = conf.env.cxxshlib_PATTERN = '%s.qdp'

    msvc = 'msvc' in conf.env.CC_NAME
    if msvc:
        cflags = []
    else:
        cflags = ['-m32', '-march=i686', '-Wall', '-g']
    conf.env.append_value('CFLAGS', cflags)
    conf.env.append_value('CXXFLAGS', cflags)

# Lazy way to split list of paths, names is input and split_these are paths that are to be
# moved to a separate list.
# split_files(['a', 'b', 'c', 'asd'], ['a']) would return (['b', 'c'], ['a', 'asd'])
def split_files(names, split_these):
    second = []
    for name in split_these:
        second += [path for path in names if name in path.srcpath()]
        names = [path for path in names if not name in path.srcpath()]
    return names, second

def build(bld):
    if bld.options.debug and bld.options.nodebug:
        bld.fatal('Both --debug and --nodebug were specified')
    debug = (bld.env.debug or bld.options.debug) and not bld.options.nodebug
    if bld.options.static and bld.options.nostatic:
        bld.fatal('Both --static and --nostatic were specified')
    optimize = bld.env.optimize or bld.options.optimize
    static = (bld.options.static or bld.env.static) and not bld.options.nostatic
    if bld.options.console and bld.options.noconsole:
        bld.fatal('Both --console and --noconsole were specified')
    console = (bld.env.console or bld.options.console) and not bld.options.noconsole
    perf_test = bld.env.perf_test or bld.options.perf_test
    sync_test = bld.env.sync_test or bld.options.sync_test
    forced_seed = bld.options.forced_seed or bld.env.forced_seed
    msvc = 'msvc' in bld.env.CC_NAME
    clang = 'clang' in bld.env.CXX_NAME

    cflags = []
    cxxflags = []
    except_cxxflags = []
    noexcept_cxxflags = []
    linkflags = []
    defines = ['NOMINMAX']
    noexcept_defines = []
    includes = []
    libs = []
    stlibs = []
    libpath = []
    stlibpath = []
    if not msvc:
        cflags += ['-Wno-format']
        if bld.env.CXX_NAME == 'gcc':
            cflags += ['-Wno-strict-overflow', '-Wno-sign-compare']
        # Only msvc works without optimizations atm
        #if perf_test or not debug or optimize:
        cflags += ['-O3']
        cxxflags += ['--std=gnu++14']
        noexcept_cxxflags += ['-fno-exceptions']
        linkflags += ['-m32', '-pthread', '-Wl,--shared']
        if debug:
            linkflags += ['-Wl,--image-base=0x42300000']
        else:
            cflags += ['-ffunction-sections', '-fdata-sections']
            linkflags += ['-Wl,--gc-sections']
        if static:
            linkflags += ['-static']
            bld.env.SHLIB_MARKER = ''
    else:
        if perf_test or not debug or optimize:
            cflags += ['/Ox']
        libs += ['user32']
        except_cxxflags += ['/EHsc']
        noexcept_defines += ['_HAS_EXCEPTIONS=0']
        cxxflags += ['/wd4624'] # Silence a seemingly incorrect warning in game.cpp
        cxxflags += ['/Zi', '/FS']
        linkflags += ['/DEBUG']
        if not debug:
            linkflags += ['/OPT:REF', '/OPT:ICF']

    if debug:
        defines += ['DEBUG']

    if forced_seed:
        defines += ['STATIC_SEED={}'.format(forced_seed)]
    #if perf_test or not debug:
        #cflags += ['-flto']
        #linkflags += ['-flto']

    if console:
        if not bld.env.freetype_pkg:
            libs += ['freetype']
        defines += ['CONSOLE']
        ft_inc = bld.options.ft_inc or bld.env.ft_inc
        ft_lib = bld.options.ft_lib or bld.env.ft_lib
        ft_link = bld.options.ft_link or bld.env.ft_link
        if ft_inc:
            node = bld.path.find_node(ft_inc)
            if node:
                includes += [node.abspath()]
            else:
                includes += [ft_inc]
        if ft_lib:
            node = bld.path.find_node(ft_lib)
            if node:
                libpath += [node.abspath()]
            else:
                libpath += [ft_lib]
        if ft_link:
            # Obv ugly
            ft_libs = filter(lambda x: x.startswith('-l'), ft_link.split(' '))
            linkflags += list(filter(lambda x: not x.startswith('-l'), ft_link.split(' ')))
            libs += list(map(lambda x: x[2:], ft_libs))
    if perf_test:
        defines += ['PERFORMANCE_DEBUG']
    if sync_test:
        defines += ['SYNC']
    if static:
        stlibs = libs
        stlibpath = libpath
        libs = []
        libpath = []

    cxxflags += cflags

    src_with_exceptions = ['save.cpp', 'patchmanager.cpp']

    src = bld.path.ant_glob('src/*.cpp')
    src += bld.path.ant_glob('src/console/*.cpp')
    src += bld.path.ant_glob('src/common/*.cpp')
    src, exception_src = split_files(src, ['save', 'patchmanager'])
    src, debug_src = split_files(src, ['test_game'])
    # Have to console.cpp as split_files does just a simple substring match atm
    # and it would match all files in console (No, the files aren't organized logically)
    src, console_src = split_files(src, ['scconsole', 'console.cpp', 'cmdargs', 'genericconsole', 'font'])
    if console:
        src += console_src
    if debug:
        src += debug_src

    includes += [bld.bldnode.find_dir('src')]
    if msvc:
        bld(rule = 'py -3 ${SRC} ${TGT} --msvc', source = ['src/func/genfuncs.py', 'src/func/nuottei.txt'], target = 'src/funcs.autogen')
    elif clang:
        bld(rule = 'py -3 ${SRC} ${TGT} --clang', source = ['src/func/genfuncs.py', 'src/func/nuottei.txt'], target = 'src/funcs.autogen')
    else:
        bld(rule = 'py -3 ${SRC} ${TGT}', source = ['src/func/genfuncs.py', 'src/func/nuottei.txt'], target = 'src/funcs.autogen')

    if msvc:
        def_file = ['msvc.def'] # Why doesn't msvc allow @4 prefix on stdcall ???
    else:
        def_file = ['gcc.def']

    bld.shlib(source='', linkflags=linkflags, target='teippi', use='obj_with_exceptions obj',
        uselib='freetype', defs=def_file, features='cxx cxxshlib', install_path=None, lib=libs, stlib=stlibs,
        libpath=libpath, stlibpath=stlibpath)

    bld.objects(source=src, cflags=cflags, cxxflags=cxxflags + noexcept_cxxflags,
            defines=defines + noexcept_defines, includes=includes, target='obj')
    bld.objects(source=exception_src, cxxflags=cxxflags + except_cxxflags, defines=defines, includes=includes, target='obj_with_exceptions')
    if not msvc:
        bld(rule='objcopy -S ${SRC} ${TGT}', source='teippi.qdp', target='teippi_stripped.qdp')

    bld.install_as('${PREFIX}/teippi.qdp', 'teippi_stripped.qdp')
