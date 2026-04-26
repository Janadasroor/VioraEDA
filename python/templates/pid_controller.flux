# PID Controller Template
# INPUTS: setpoint, feedback
# OUTPUTS: out

let setpoint = inputs[0] in
let feedback = inputs[1] in
let error = setpoint - feedback in
let Kp = 1.5 in
error * Kp
