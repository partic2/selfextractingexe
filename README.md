# selfextractingexe

This is a tool to pack directory into one executable, which can extract these packed files out when executed.  
Support Windows and Linux

## Usage

run 
```
selfextractingexe pack
```
to pack directory that contains **selfextractingexe** self.

The generated **selfextractingexe_packed** can be directly run to extract files. And if "autorun" existed, pass it's content to **system** call. 

NOTE: You may need add selfextractingexe_packed executable flag manually on linux.  
NOTE: On windows, wide-char will cause error, Maybe fixed in future.  

## Build

```sh
c++ -o selfextractingexe -s -static selfextractingexe.cpp
```