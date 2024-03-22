#### build and use
1. Clone the repo. 
2. This sln depend on `https://github.com/yunate/dd`, so clone it in the same dir. The directory structure is roughly like the following:
```
your dir
|--- dd
|--- ddimage
|    |--- projects
|    |    |--- ddimage          // Output ddimage.lib
|    |    |--- ddimage_test     // Output ddimage_test.exe, there are some demos on how to use ddimage.
```
3. `ddimage` depends on `FreeImage` and is managed using `VCPKG`, so you need to install `VCPKG` first:
```
git clone https://github.com/microsoft/vcpkg.git
bootstrap-vcpkg.bat
vcpkg integrate install
```
4. Build `dd` first(https://github.com/yunate/dd).
5. Open the `ddimage.sln` with Visual studio, and build it yourself.
6. Add head file dir: `dd\projects; ddimage\projects`, and add the head file in your code `#include "ddimage/ddimage.h"`
7. Add the library dir: `dd\bin\$(Configuration)_$(PlatForm)\; ddimage\bin\$(Configuration)_$(PlatForm)\`, and add the needed lib `ddbase.lib; ddimage.lib`.
8. This project depends on FreeImage. Refer to `projects\ddimage_test\vcpkg.json` `vcpkg-configuration.json` files for configuring your FreeImage dependency, you can just copy those file to your own project dir, and configure VS with UseVcpkg set to true. 
9. You can refer to the demo in the test project for usage.
#### notes
1. Due to the project's use of the STL library in its exported items, it is not recommended to directly send the header files and compiled lib/dll files to others for use. Instead, it is advised to compile and use them locally on your own machine.