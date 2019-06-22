# stgen_1to2

This is a utility for converting the old SynchroTrace format, compatible with v1,
to the newer format, compatible with v2.

# Build

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

# Run

This will overwrite any traces with the same name in the target directory!

```
./stgen_1to2 DIR_WITH_V1_TRACES DIR_WITH_V2_TRACES
```

