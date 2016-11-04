//@ #include "../annotated_api/general_definitions/cryptogram.gh"

/*@

lemma_auto void cryptogram()
  requires [?f]cryptogram(?array, ?count, ?ccs, ?cg);
  ensures  [f]cryptogram(array, count, ccs, cg) &*&
           ccs == ccs_for_cg(cg) && cg_is_generated(cg); 
{
  open [f]cryptogram(array, count, ccs, cg);
  close [f]cryptogram(array, count, ccs, cg);
}
 
lemma_auto void cryptogram_inv()
  requires [?f]cryptogram(?array, ?count, ?cs, ?cg);
  ensures  [f]cryptogram(array, count, cs, cg) &*& length(cs) == count;
{
  open [f]cryptogram(array, count, cs, cg);
  close [f]cryptogram(array, count, cs, cg);
}

lemma void cryptogram_limits(char *array)
  requires [?f]cryptogram(array, ?count, ?cs, ?cg) &*&
           true == ((char *)0 <= array) &*& array <= (char *)UINTPTR_MAX;
  ensures  [f]cryptogram(array, count, cs, cg) &*&
           true == ((char *)0 <= array) &*& array + count <= (char *)UINTPTR_MAX;
{
  open [f]cryptogram(array, count, cs, cg);
  crypto_chars_limits(array);
  close [f]cryptogram(array, count, cs, cg);
}

@*/
