import math

T0 = 298.15
R0 = 10000.0
BETA = 3694.0
R_PULLUP = 10000.0

user_data = """10	18787
15	15136
20	12268
25	10000
30	8196
40	5594
50	3893
60	2760
70	1990
80	1458
85	1255"""

print(f"| True Temp (°C) | Resistance (Ω) | Beta=3694 Temp (°C) | Error (°C) |")
print(f"|----------------|----------------|---------------------|------------|")

for line in user_data.split('\n'):
    if not line.strip(): continue
    parts = line.split('\t')
    t_true = float(parts[0])
    r_true = float(parts[1])
    
    inv_t = (1.0 / T0) + (1.0 / BETA) * math.log(r_true / R0)
    t_beta = (1.0 / inv_t) - 273.15
    error = t_beta - t_true
    
    print(f"| {t_true:.1f} | {r_true:.0f} | {t_beta:.2f} | {error:+.2f} |")
