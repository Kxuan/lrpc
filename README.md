# lrpc
[![Build Status](https://travis-ci.org/Kxuan/lrpc.svg?branch=master)](https://travis-ci.org/Kxuan/lrpc)

lrpc is a Linux-only lightweight rpc library. The rpc is send between process to process, and do not need a third process.

# Dependency
- uthash  (dev dependency)
- check (dev dependency)

# Build
```bash
mkdir build
cd build
cmake ..
make
make test
make install
```

# Documention
The api are not stable yet. Please refer to the example in the `examples` directory.

# License
LGPLv3