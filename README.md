# Samoyed

## Compile LLVM frontend

1. Compile the Samoyed frontend written as an LLVM frontend.
> cd ./ext/jit/JitFrontend/bld/

> make

## Compile code

0. Samoyed compilation is done in two-phase. I kept it separate for easy debugging.
1. Write code using Samoyed language (e.g., look under frontend_code/ for example).
2. Translate it into LLVM-parsible code.
> cd frontend_code

> python3 frontend_translator.py add_inplace_jit.c
3. Move the resulting code under src/
> mv add_inplace_jit_new.c ../src/add_inplace_jit.c
4. Compile with LLVM-toolchain.
> cd ..

> ./compile.sh jit add_inplace

## Flash

> mspdebug tilib "prog bld/jit/add_inplace_jit.out"
