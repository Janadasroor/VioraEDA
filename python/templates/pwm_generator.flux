# PWM Generator Template
# INPUTS: duty_v
# OUTPUTS: out

# Normalizes 0..1V input to 0..100% duty
let raw = inputs[0] in
let duty = if raw > 1.0 then 1.0 else (if raw < 0.0 then 0.0 else raw) in

let freq = 1000.0 in
let period = 1.0 / freq in
let local_t = t - (floor(t / period) * period) in

# Output 5V pulses
if local_t < (period * duty) then 5.0 else 0.0
