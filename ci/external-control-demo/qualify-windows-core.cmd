@echo off
setlocal enabledelayedexpansion
if not exist evidence mkdir evidence
call buildwin\win_deps.bat wx32
if not "!errorlevel!"=="0" exit /b 1
call cache\wx-config.bat
if not "!errorlevel!"=="0" exit /b 1
set "PATH=!wxWidgets_LIB_DIR!;%PROGRAMFILES%\Poedit\Gettexttools\bin;%PATH%"
cmake -S . -B build-platform -G "Visual Studio 17 2022" -A Win32 ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DwxWidgets_LIB_DIR=!wxWidgets_LIB_DIR! ^
  -DwxWidgets_ROOT_DIR=!wxWidgets_ROOT_DIR! ^
  -DwxWidgets_CONFIGURATION=mswu ^
  -DOCPN_TARGET_TUPLE="msvc-wx32;10;x86" ^
  -DOCPN_CI_BUILD=ON -DOCPN_RELEASE=0 ^
  -DOCPN_USE_VULKAN_PRESENTER=OFF -DOCPN_USE_GL=ON ^
  -DOCPN_BUNDLE_WXDLLS=ON -DOCPN_BUNDLE_VCDLLS=OFF ^
  -DOCPN_BUILD_TEST=ON
if not "!errorlevel!"=="0" exit /b 1
cmake --build build-platform --config RelWithDebInfo --parallel 3
if not "!errorlevel!"=="0" exit /b 1
rem Resolve the same native dependencies as the core without the old vc/ DLLs.
set "PATH=%CD%\cache\buildwin;%CD%\build-platform\RelWithDebInfo;%PATH%"
dumpbin /dependents build-platform\test\RelWithDebInfo\tests.exe > evidence\test-dependencies.txt
build-platform\test\RelWithDebInfo\tests.exe ^
  --gtest_filter=ExternalApiTest.*:InProcessPlanningJobServiceTest.*:BoundedApplicationEventStreamTest.*:ChartSafetyDepth.*:ChartSafetyService.* ^
  --gtest_output=xml:evidence/core-tests.xml > evidence\tests.log 2>&1
set "test_exit=!errorlevel!"
type evidence\tests.log
if not "!test_exit!"=="0" exit /b !test_exit!
python ci\external-control-demo\verify-test-report.py evidence\core-tests.xml
if not "!errorlevel!"=="0" exit /b 1
dumpbin /headers build-platform\RelWithDebInfo\opencpn.exe > evidence\architecture.txt
dumpbin /dependents build-platform\RelWithDebInfo\opencpn.exe > evidence\dependencies.txt
git rev-parse HEAD > evidence\source-commit.txt
