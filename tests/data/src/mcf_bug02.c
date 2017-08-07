
/* based on sort_basket() in 429.mcf from SPEC CPU2006 v1.1 */

int basket[10];

int test() {
  int l = 0, r = sizeof(basket), min, max;
  int *perm;
  int xchange;

  do {
    if (l < r) {
      xchange = perm[l];
      perm[l] = perm[r];
      perm[r] = xchange;
    }
    if (l <= r) {
      l++;
      r--;
    }

  } while (l <= r);

  if (r < min)
    return r;
  else
    return max;
}
