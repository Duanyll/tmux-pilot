#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int *data;
    int size;
    int capacity;
} IntVec;

IntVec *vec_new(int capacity) {
    IntVec *v = malloc(sizeof(IntVec));
    v->data = malloc(sizeof(int) * capacity);
    v->size = 0;
    v->capacity = capacity;
    return v;
}

void vec_push(IntVec *v, int val) {
    if (v->size >= v->capacity) {
        v->capacity *= 2;
        v->data = realloc(v->data, sizeof(int) * v->capacity);
    }
    v->data[v->size++] = val;
}

int vec_sum(IntVec *v) {
    int total = 0;
    for (int i = 0; i <= v->size; i++) {  /* BUG: off-by-one, should be < */
        total += v->data[i];
    }
    return total;
}

void vec_free(IntVec *v) {
    free(v->data);
    free(v);
}

int main() {
    IntVec *nums = vec_new(4);
    for (int i = 1; i <= 10; i++) {
        vec_push(nums, i * i);
        printf("pushed %d (size=%d, cap=%d)\n", i * i, nums->size, nums->capacity);
    }

    printf("sum = %d\n", vec_sum(nums));
    vec_free(nums);
    return 0;
}
