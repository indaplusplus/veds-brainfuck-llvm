#include <stdio.h>
#include <stdlib.h>

#define BF_MEMORY_SIZE 100000

char* read_program(char *filename) {
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
                                   
    buffer[file_size] = '\0';
    fclose(f);
    return buffer;
}

void interprete(char *program) {
    char *mem = calloc(BF_MEMORY_SIZE, 1);
    char *ptr = mem;
    char c;
    int loop = 0;
    while ((c = *program) != '\0') {
        switch(c) {
            case '+':
                (*ptr)++;
            break;
            case '-':
                (*ptr)--;
            break;
            case '>':
                ptr++;
            break;
            case '<':
                ptr--;
            break;
            case '.':
                putchar(*ptr);
            break;
            case ',':
                *ptr = getchar();
            break;
            case '[':
                if (!*ptr) {
                    loop = 1;
                    while(loop) {
                        *program++;
                        if (*program == ']') {
                            --loop;
                        } else if (*program == '[') {
                            ++loop;
                        }
                    }
                }
            break;
            case ']':
                if (*ptr) {
                    loop = 1;
                    while(loop) {
                        *program--;
                        if (*program == ']') {
                            ++loop;
                        } else if (*program == '[') {
                            --loop;
                        }
                    }
                }
            break;
        }
        program++;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2)
        return 0;
    char *program = read_program(argv[1]);
    interprete(program);
}
