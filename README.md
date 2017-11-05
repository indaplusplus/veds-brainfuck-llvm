# Dependencies
* clang-4.0 
* lldb-4.0

# Install
Compile the brainfuck-llvm compiler with:
```
make
```

# Usage
To compile a brainfuck script, in this case file.bf, run:
```
./brainfuck_cc file.bf && make compile
```

There should now exist a ```bf_cc_output``` binary in your directory.
