user_data = """10	18787
11	17983
12	17219
13	16490
14	15797
15	15136
16	14506
17	13906
18	13334
19	12788
20	12268
21	11771
22	11297
23	10845
24	10413
25	10000
26	9606
27	9229
28	8869
29	8525
30	8196
31	7882
32	7581
33	7293
34	7018
35	6754
36	6501
37	6260
38	6028
39	5806
40	5594
41	5390
42	5195
43	5007
44	4828
45	4656
46	4490
47	4332
48	4180
49	4034
50	3893
51	3759
52	3629
53	3505
54	3386
55	3271
56	3160
57	3054
58	2952
59	2854
60	2760
61	2669
62	2582
63	2498
64	2417
65	2339
66	2264
67	2191
68	2122
69	2055
70	1990
71	1928
72	1868
73	1810
74	1754
75	1700
76	1648
77	1598
78	1550
79	1503
80	1458
81	1414
82	1372
83	1332
84	1293
85	1255"""

res_list = []
for line in user_data.split('\n'):
    if not line.strip(): continue
    parts = line.split('\t')
    res_list.append(parts[1])

print("#define GA10K_TABLE_SIZE 76")
print("static const float ga10k_res_table[GA10K_TABLE_SIZE] = {")
for i in range(0, len(res_list), 8):
    chunk = res_list[i:i+8]
    formatted = ", ".join([f"{val}.0f" for val in chunk])
    if i + 8 < len(res_list):
        formatted += ","
    print(f"    {formatted}")
print("};")
