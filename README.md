# Teippi
Bw modhack thingy which removes several limits from the game and makes it run faster

# Building

The simplest way to build the project is to open `teippi.sln` in Visual Studio 2015 Update 2
or newer, and build the project. If everything goes well, the plugin will be either in `Debug/`,
`FastDebug/` or `Release/` directory. Do note that the default Debug build can be really slow.

Teippi can also be built with gcc/clang, but that requires having Python 3 to run the build script:

Run `py -3 waf configure` followed by `py -3 waf` to build the plugin. The resulting file will be in build\teippi.qdp, which can be renamed to teippi.bwl to use it in Chaoslauncher.

For compile options, see `py -3 waf --help`
