# PID Controller Template (Clean)
# INPUTS: setpoint, feedback
# OUTPUTS: out

error = setpoint - feedback
Kp = 1.5
error * Kp
