; This program tests writing and reading array information as bits to the Koyo PLC

; Read array of bits
t = caget('KOYO1:CnInBArray', cnb)

; Set bits 128, 129 high then low, waiting 1 second
cnb[128]=1
t = caput('KOYO1:CnOutBArray', cnb)
wait, 1

cnb[129]=1
t = caput('KOYO1:CnOutBArray', cnb)
wait, 1

cnb[128]=0
t = caput('KOYO1:CnOutBArray', cnb)
wait, 1

cnb[129]=0
t = caput('KOYO1:CnOutBArray', cnb)
wait, 1

end

