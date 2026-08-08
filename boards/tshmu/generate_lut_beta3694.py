import math

VREF = 3.3
R_PULLUP = 10000.0
R0 = 10000.0
T0 = 298.15
BETA = 3694.0

lut = []
for i in range(65):
    # Map index to voltage
    v_out = (i / 64.0) * VREF
    
    # Avoid division by zero
    if v_out <= 0.001:
        temp_c = 125.0
    elif v_out >= VREF - 0.001:
        temp_c = -40.0
    else:
        # R_therm = R_pullup * Vout / (Vref - Vout)
        r_therm = R_PULLUP * v_out / (VREF - v_out)
        
        # Temp in Kelvin using Beta
        inv_t = (1.0 / T0) + (1.0 / BETA) * math.log(r_therm / R0)
        temp_c = (1.0 / inv_t) - 273.15
        
        # Cap temps
        if temp_c > 125.0: temp_c = 125.0
        if temp_c < -40.0: temp_c = -40.0
        
    lut.append(temp_c)

# Format the output like the C array
print("static const float adc_to_temp_lut[LUT_ARRAY_SIZE] = {")
for i in range(0, 65, 8):
    chunk = lut[i:i+8]
    formatted = ", ".join([f"{val:6.2f}f" for val in chunk])
    if i + 8 < 65:
        formatted += ","
    print(f"    {formatted} // {i} - {i+len(chunk)-1}")
print("};")
