# SPWM Generator (Sinusoidal PWM)
# Logic: Compare a 50Hz Sine wave with a 10kHz Triangle wave

# Parameters
f_sine = 50.0
f_carrier = 10000.0
modulation_index = 0.8

# 1. Generate Sine Reference (0.0 to 1.0 range)
sine_ref = (sin(2 * 3.14159 * f_sine * t) * 0.5 * modulation_index) + 0.5

# 2. Generate Triangle Carrier (0.0 to 1.0 range)
period = 1.0 / f_carrier
local_t = t - (floor(t / period) * period)
tri_carrier = if local_t < (period / 2.0) then (local_t / (period / 2.0)) else (1.0 - (local_t - period / 2.0) / (period / 2.0))

# 3. Comparator Logic (Output 5V if sine > carrier)
if sine_ref > tri_carrier then 5.0 else 0.0