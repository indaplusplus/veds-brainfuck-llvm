#include <stdio.h>
#include <stdlib.h>
//llvm 4.0 (currently stable) - https://releases.llvm.org/4.0.0/docs/index.html
#include <llvm-c/Core.h>
#include <llvm-c/BitWriter.h>

#define BF_STACK_SIZE 100000

typedef struct {
    /**
     * relying on a few libc functions to make life easier.
     */
    LLVMValueRef calloc_func;   //calloc since we need to zero-initialize the array.
    LLVMValueRef putchar_func;
    LLVMValueRef getchar_func;

    LLVMBuilderRef builder;
    LLVMValueRef main;

    LLVMValueRef bf_data;
    LLVMValueRef bf_ptr;
}brainfuck;

typedef struct {
    LLVMBasicBlockRef condition;
    LLVMBasicBlockRef body;
    LLVMBasicBlockRef end;
}brainfuck_loop;

char *read_bf_file(char *filename) {
    FILE *f = fopen(filename, "r");
    
    if (f == NULL) {
        abort();
    }

    fseek(f, 0L, SEEK_END); //move to the end
    size_t file_size = ftell(f);
    rewind(f);

    char *buffer = malloc(sizeof(char) * file_size + 1);
    fread(buffer, sizeof(char), file_size, f);
    
    buffer[file_size + 1] = '\0'; //string terminate buffer
    fclose(f);

    return buffer;
}

void llvm_brainfuck_inc_data(brainfuck *bf, int val) {
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

void llvm_brainfuck_dec_data(brainfuck *bf, int val) {
    llvm_brainfuck_inc_data(bf, -val);
}

void llvm_brainfuck_inc_ptr(brainfuck *bf, int val) {
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

void llvm_brainfuck_dec_ptr(brainfuck *bf, int val) {
    llvm_brainfuck_inc_ptr(bf, -val);
}

void llvm_brainfuck_putchar(brainfuck *bf) {
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

void llvm_brainfuck_getchar(brainfuck *bf) {
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

void llvm_brainfuck_start_loop(brainfuck *bf, brainfuck_loop *loop) {
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

void llvm_brainfuck_end_loop(brainfuck *bf, brainfuck_loop *loop) {
    LLVMBuildBr(bf->builder, loop->condition);
    LLVMPositionBuilderAtEnd(bf->builder, loop->end);
}

void interprete_brainfuck(char *bf_program, brainfuck *bf) {
    brainfuck_loop loops[1000];
    brainfuck_loop *loop_ptr = loops;
    
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
                loop_ptr++;
                llvm_brainfuck_start_loop(bf, loop_ptr);
                
            break;

            case ']':
                llvm_brainfuck_end_loop(bf, loop_ptr);
                loop_ptr--;
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
    
    brainfuck *bf = malloc(sizeof(brainfuck));
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
        LLVMPointerType(LLVMInt32Type(), 0), 
        getchar_types, 
        0, 
        0
    ); 
    LLVMValueRef getchar = LLVMAddFunction(module, "getchar", getchar_type);
    bf->getchar_func = getchar;


    /*
     * allocate the brainfuck stack
     */
    LLVMValueRef calloc_args[2] = {
        LLVMConstInt(LLVMInt32Type(), BF_STACK_SIZE, 0),
        LLVMConstInt(LLVMInt32Type(), 1, 0)
    }; 
    
    LLVMValueRef data_ptr = LLVMBuildCall(builder, calloc, calloc_args, 2, "calloc_data_ptr");
    bf->bf_data = LLVMBuildAlloca(builder, LLVMPointerType(LLVMInt32Type(), 0), "data");
    bf->bf_ptr = LLVMBuildAlloca(builder, LLVMPointerType(LLVMInt32Type(), 0), "ptr");

    LLVMBuildStore(builder, data_ptr, bf->bf_data);
    LLVMBuildStore(builder, data_ptr, bf->bf_ptr);

    interprete_brainfuck(read_bf_file(bf_filename), bf);

    LLVMBuildRet(builder, one);
    LLVMWriteBitcodeToFile(module, "brainfuck.bc");
    LLVMDisposeBuilder(builder);
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        llvm_brainfuck(argv[1]);
    }
    return 0;
}
