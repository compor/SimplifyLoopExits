
/* based on global_opt() in 429.mcf from SPEC CPU2006 v1.1 */

int net;

int price_out_impl(int);

int test(void) {
  int new_arcs;
  int n;

  new_arcs = -1;
  n = 5;

  while (new_arcs) {
    if (!n)
      break;

    new_arcs = price_out_impl(net);

    if (new_arcs < 0) {
      break;
    }

    n--;
  }

  return 0;
}
