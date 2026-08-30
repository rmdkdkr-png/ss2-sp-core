flip = open("svc_S_flip.ram", "rb").read()
c00 = open("svc_S_c0_0.ram", "rb").read()
f2 = open("svc_S_f2.ram", "rb").read()   # P1=Kyo(0) P2=? left/right normal
N = len(flip)
print("flip 08A0=%d 08C0=%d | c0_0 08A0=%d 08C0=%d" % (flip[0x08A0], flip[0x08C0], c00[0x08A0], c00[0x08C0]))
# actor struct stride 0x180 ; find field where slot-A id matches the pad-controlled char
hits = []
for o in range(N - 0x180):
    if flip[o] == 9 and flip[o + 0x180] == 0 and c00[o] == 0 and c00[o + 0x180] == 10:
        hits.append(o)
print("fields where slotA=pad-controlled char id (stride 0x180):", ["%04X" % o for o in hits])
# also: does any pair (a,b) anywhere hold (9,0) in flip and (0,10) in c0_0 ?
cand = [o for o in range(N) if flip[o] == 9 and c00[o] == 0]
cand2 = [o for o in range(N) if flip[o] == 0 and c00[o] == 10]
print("slotA-id candidates:", ["%04X" % o for o in cand][:30])
print("slotB-id candidates:", ["%04X" % o for o in cand2][:30])
