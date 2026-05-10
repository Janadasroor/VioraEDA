adevice L1 [d, en, q] q {
    d_val = inputs[0] > 2.5;
    en_val = inputs[1] > 2.5;
    q_prev = inputs[2] > 2.5;
    if en_val then d_val else q_prev
}
