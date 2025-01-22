# CWaggle

A Waggle library for C applications.

## Building with CWaggle

1. Build the library
```
make
```

or

```
mkdir build && cd build
cmake ..
make install
```

2. Build your application with CWaggle
```
gcc -o myapp myapp.c -I./include -L. -lwaggle -lcjson -lrabbitmq -lpthread
```
