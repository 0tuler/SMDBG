#include "../SMDBG.h"

int main() {
  const size_t vector_size = 10;
  int *pa = (int *)malloc(sizeof(int) * vector_size);

  while (1) {
    int *p = malloc(4);
  }

  // free(pa);

  return 0;
}
