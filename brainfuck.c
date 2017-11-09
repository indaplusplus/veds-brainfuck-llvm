/*
 * Uses llvm 4.0 (currently stable) -
 * https://releases.llvm.org/4.0.0/docs/index.html
 */
#include <stdio.h>
#include <stdlib.h>
#include <llvm-c/Core.h>
#include <llvm-c/BitWriter.h>

#define BF_MEMORY_SIZE 100000

typedef struct {
    /**
     * relying on a few libc functions to make life easier.
     */
    LLVMValueRef calloc_func;   //calloc since we need to zero-initialize the array.
    LLVMValueRef putchar_func;
    LLVMValueRef getchar_func;
    LLVMBuilderRef builder;
    LLVMValueRef main;
    LLVMValueRef bf_ptr;
}Brainfuck;

typedef struct {
    LLVMBasicBlockRef condition;
    LLVMBasicBlockRef body;
    LLVMBasicBlockRef end;
}Brainfuck_loop;

char *read_bf_file(char *filename) {
    FILE *f = fopen(filename, "r");

    if (f == NULL) {
        abort();
    }

    fseek(f, 0L, SEEK_END);
    size_t file_size = ftell(f);
    rewind(f);

    char *buffer;
    if ((buffer = malloc(sizeof(char) * file_size + 1)) == NULL) {
        abort();
    }
    fread(buffer, sizeof(char), file_size, f);

    //string terminate buffer
    buffer[file_size + 1] = '\0';
    fclose(f);

    return buffer;
}

void llvm_brainfuck_inc_data(Brainfuck *bf, int val) {
    LLVMValueRef one = LLVMConstInt(LLVMInt32Type(), val, 0);

    LLVMValueRef tmp = LLVMBuildLoad(bf->builder, bf->bf_ptr, "tmp_inc_data");
    LLVMBuildStore(
        bf->builder,
        LLVMBuildAdd(
            bf->builder,
            LLVMBuildLoad(bf->builder, tmp, "tmp_inc_data"),
            one,
            "tmp_inc"
        ),
        tmp
    );
}

void llvm_brainfuck_dec_data(Brainfuck *bf, int val) {
    llvm_brainfuck_inc_data(bf, -val);
}

void llvm_brainfuck_inc_ptr(Brainfuck *bf, int val) {
    LLVMValueRef one = LLVMConstInt(LLVMInt32Type(), val, 0);
    LLVMValueRef indices[1] = { one };

    LLVMBuildStore(
        bf->builder,
        LLVMBuildInBoundsGEP(
            bf->builder,
            LLVMBuildLoad(bf->builder, bf->bf_ptr, "asdf"),
            indices,
            1,
            "inc_ptr"
        ),
        bf->bf_ptr
    );
}

void llvm_brainfuck_dec_ptr(Brainfuck *bf, int val) {
    llvm_brainfuck_inc_ptr(bf, -val);
}

void llvm_brainfuck_putchar(Brainfuck *bf) {
    LLVMValueRef tmp = LLVMBuildLoad(bf->builder, bf->bf_ptr, "tmp_putchar");

    LLVMValueRef putchar_args[] = {
        LLVMBuildSExt(
            bf->builder,
            LLVMBuildLoad(bf->builder, tmp, "cccc"),
            LLVMInt32Type(),
           "bbbb"
        )
    };

    LLVMBuildCall(
        bf->builder,
        bf->putchar_func,
        putchar_args,
        1,
        "putchar"
    );
}

void llvm_brainfuck_getchar(Brainfuck *bf) {
    LLVMBuildStore(
        bf->builder,
        LLVMBuildTrunc(
            bf->builder,
            LLVMBuildCall(
                bf->builder,
                bf->getchar_func,
                NULL,
                0,
                "getchar"
            ),
            LLVMInt32Type(),
            "fucknamingthisshit"
        ),
        LLVMBuildLoad(bf->builder, bf->bf_ptr, "tmp_getchar")
    );
}

void llvm_brainfuck_start_loop(Brainfuck *bf, Brainfuck_loop *loop) {
    loop->condition = LLVMAppendBasicBlock(bf->main, "condition");
    loop->body = LLVMAppendBasicBlock(bf->main, "body");
    loop->end = LLVMAppendBasicBlock(bf->main, "end");

    LLVMBuildBr(bf->builder, loop->condition);
    LLVMPositionBuilderAtEnd(bf->builder, loop->condition);

    LLVMValueRef tmp = LLVMBuildLoad(bf->builder, bf->bf_ptr, "tmp_start_loop");

    LLVMIntPredicate cmpne = LLVMIntNE;
    LLVMBuildCondBr(
        bf->builder,
        LLVMBuildICmp(
            bf->builder,
            cmpne,
            LLVMBuildLoad(
                bf->builder,
                tmp,
                "load_loop"
            ),
            LLVMConstInt(LLVMInt32Type(), 0, 0), //0 from second arg
            "idkfam"
        ),
        loop->body,
        loop->end
    );
    LLVMPositionBuilderAtEnd(bf->builder, loop->body);
}

void llvm_brainfuck_end_loop(Brainfuck *bf, Brainfuck_loop *loop) {
    LLVMBuildBr(bf->builder, loop->condition);
    LLVMPositionBuilderAtEnd(bf->builder, loop->end);
}

void interprete_brainfuck(char *bf_program, Brainfuck *bf) {
    Brainfuck_loop loops[1000]; //change to a linked list
    Brainfuck_loop *loop_ptr = loops;

    char c;
    while((c = *bf_program) != '\0') {
        switch (c) {
            case '+':
                llvm_brainfuck_inc_data(bf, 1);
            break;

            case '-':
                llvm_brainfuck_dec_data(bf, 1);
            break;

            case '>':
                llvm_brainfuck_inc_ptr(bf, 1);
            break;

            case '<':
                llvm_brainfuck_dec_ptr(bf, 1);
            break;

            case '.':
                llvm_brainfuck_putchar(bf);
            break;

            case ',':
                llvm_brainfuck_getchar(bf);
            break;

            case '[':
                llvm_brainfuck_start_loop(bf, loop_ptr);
                loop_ptr++;
            break;

            case ']':
                loop_ptr--;
                llvm_brainfuck_end_loop(bf, loop_ptr);
            break;
        }
        bf_program++;
    }
    //TODO - check if all loops have been closed.
}

void llvm_brainfuck(char *bf_filename) {
    LLVMModuleRef module = LLVMModuleCreateWithName("bf_llvm");
    LLVMTypeRef main_ret = LLVMFunctionType(LLVMInt32Type(), NULL, 0, 0);
    LLVMValueRef main_value_ref = LLVMAddFunction(module, "main", main_ret);

    LLVMBasicBlockRef main = LLVMAppendBasicBlock(main_value_ref, "bf_main");
    LLVMBuilderRef builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(builder, main);

    Brainfuck *bf;
    if ((bf = malloc(sizeof(Brainfuck))) == NULL) {
        abort();
    }

    bf->builder = builder;
    bf->main = main_value_ref;

    LLVMValueRef one = LLVMConstInt(LLVMInt32Type(), 1, 0);

    /*
     * setup calloc
     */
    LLVMTypeRef calloc_types[] = { LLVMInt32Type(), LLVMInt32Type() };
    LLVMTypeRef calloc_type = LLVMFunctionType(
        LLVMPointerType(LLVMInt32Type(), 0),
        calloc_types,
        0,
        1
    );
    LLVMValueRef calloc = LLVMAddFunction(module, "calloc", calloc_type);

    bf->calloc_func = calloc;

    /*
     * setup putchar
     */
    LLVMTypeRef putchar_types[] = {LLVMInt32Type()};
    LLVMTypeRef putchar_type = LLVMFunctionType(
        LLVMPointerType(LLVMInt32Type(), 0),
        putchar_types,
        0,
        1
    );
    LLVMValueRef putchar = LLVMAddFunction(module, "putchar", putchar_type);
    bf->putchar_func = putchar;

    /*
     * setup getchar
     */
    LLVMTypeRef getchar_types[] = {LLVMInt32Type()};
    LLVMTypeRef getchar_type = LLVMFunctionType(
        LLVMInt32Type(),
        getchar_types,
        0,
        0
    );
    LLVMValueRef getchar = LLVMAddFunction(module, "getchar", getchar_type);
    bf->getchar_func = getchar;


    /*
     * allocate memory for brainfuck
     */
    LLVMValueRef calloc_args[2] = {
        LLVMConstInt(LLVMInt32Type(), BF_MEMORY_SIZE, 0),
        LLVMConstInt(LLVMInt32Type(), 1, 0)
    };

    LLVMValueRef data_ptr = LLVMBuildCall(builder, calloc, calloc_args, 2, "calloc_data_ptr");
    bf->bf_ptr = LLVMBuildAlloca(builder, LLVMPointerType(LLVMInt32Type(), 0), "ptr");

    LLVMBuildStore(builder, data_ptr, bf->bf_ptr);

    char *bf_file_content = read_bf_file(bf_filename);
    interprete_brainfuck(bf_file_content, bf);

    LLVMBuildRet(builder, one);
    LLVMWriteBitcodeToFile(module, "brainfuck.bc");
    
    LLVMDisposeBuilder(builder);
    free(bf_file_content);
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        llvm_brainfuck(argv[1]);
    }
    return 0;
}
