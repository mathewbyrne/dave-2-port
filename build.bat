
@echo off

mkdir build
pushd build
cl -Fe:unpack.exe -FC -Zi -W4 -std:c17 ..\src\unpack.cpp 
popd

