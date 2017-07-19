
/* based on FChoose() in 456.hmmer of SPEC CPU2006 v1.1 */

float sre_random() {
  static int k = 0;
  static float r[] = {0.5, 0.1, 0.2, 0.05};
  int rv;

  k %= 4;
  rv = r[k];
  k++;

  return rv;
}

void test() {
  float p[6] = {1, 9, 7, 3, 4.5, 8.2};
  int N = 6;
  float roll;
  float sum;
  int i;

  roll = sre_random();
  sum = 0.0;
  for (i = 0; i < N; i++) {
    sum += p[i];
    if (roll < sum) {
      sle_print(i);
      sle_print(666);
      return;
    }
    /* uncommenting this line created the correct output */
    /*sle_print(i);*/
  }

  i = (int)(sre_random() * N);
  sle_print(i);
  sle_print(777);

  return;
}
