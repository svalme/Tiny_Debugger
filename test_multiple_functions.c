// test_multiple_functions.c
#include <stdio.h>
#include <unistd.h>

void three() {
    printf("three\n");
}

void two() {
    printf("two\n");
    three();
}

void one() {
    printf("one\n");
    two();
}

int main() {
    printf("Hello from test multiple functions program!\n");
    one();

    return 42;
}