# Teippi
Bw modhack thingy which removes several limits from the game and makes it run faster

# Building
Python 3 is required to run the build script, and either gcc 4.9.2, clang 3.5 or visual studio 2015 is required to compile the code.

Run `python waf configure` followed by `python waf` to build the plugin. The resulting file will be in build\teippi.qdp.

There is a visual studio project available which wraps around the build script.
You will have to run the configuration command before the project can build, though.

For compile options, see `python waf --help`
