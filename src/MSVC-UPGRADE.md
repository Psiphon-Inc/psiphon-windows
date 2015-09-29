# Upgrading to a new VS

Note: This is rough.

### Project Files

Copy `Client/psiclient/psiclient.201x.{sln,vcxproj,vcxproj.filters}` to `...201y...`.

Copy `Client/psiclient201x.sln` to `Client/psiclient201y.sln`. Modify the new file so that it refers to the new `psiclient201y.vcxproj` file.

### Upgrade library builds

For all: make sure that the project "Platform Toolset" is **"Visual Studio 201y - Windows XP (v1xx_xp)"**. Otherwise the client won't run on Windows XP.

#### cryptopp

In theory this should be a dependent project in our solution. But it's not.

Build the static library from source: http://www.cryptopp.com/

#### yaml-cpp

It doesn't seem possible to just add this as a dependency project, so we'll have to build it fresh each time.

1. Get [the code](https://github.com/jbeder/yaml-cpp). Maybe 0.5.x? Or whatever's newest.

2. You'll probably need to download [Boost](http://www.boost.org/), extract it, and set your `BOOST_ROOT` environment variable ([doc](http://www.boost.org/doc/libs/1_56_0/more/getting_started/windows.html)).

3. You'll need [CMake](http://www.cmake.org/). Run it, point to the yaml-cpp source, configure the desired MSVC version. Change options so that it's not shared or single-threaded (so, static and multi-threaded). Generate. Open the resulting solution in MSVC.

   The commands are something like this:
   ```
   mkdir build
   cd build
   cmake -DBUILD_SHARED_LIBS=OFF -DMSVC_SHARED_RT=OFF -G "Visual Studio 14 2015" -T "v140_xp" ..
   ```  

4. The project we need is `yaml-cpp static mt`. Check its project properties (and **fix the XP setting**, as above) -- maybe optimize for size. Build it for Debug and Release.

5. Copy the resulting `.lib` (and `.pdb`) files to new directories (see existing ones).
   * The `.pdb` is probably located in `build/yaml-cpp.dir/Debug`.

6. Alter the psiclient project settings to use the new `.lib` files.
