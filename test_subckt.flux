model my_jit_model viospice_jit(jit_id="L1")

subckt my_latch(d, en, q) {
    instance L1(d, en, q) my_jit_model
}
