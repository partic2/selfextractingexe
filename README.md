# selfextractingexe

This is a tool to pack directory into one executable, which can self extract out these packed files when executed.  
Support Windows and Linux

## Usage

run 
```
./selfextractingexe pack
```
Or just run **selfextractingexe** directly.

The generated **selfextractingexe_packed** can be directly run to extract files. And if "autorun" existed, pass it's content to **system** call. 

NOTE: You may need add selfextractingexe_packed executable flag manually on linux.  

## Build

```sh
c++ -o selfextractingexe -s -static selfextractingexe.cpp
```