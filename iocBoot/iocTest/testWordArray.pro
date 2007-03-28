; This program tests writing and reading array information as words to the Koyo PLC

; Read array of words
t = caget('KOYO1:CnInWArray', cnw)

; Set bits 128, 129 high then low, waiting 1 second
cnw[8] = 1
t = caput('KOYO1:CnOutWArray', cnw)
wait, 1

cnw[8] = 3
t = caput('KOYO1:CnOutWArray', cnw)
wait, 1

cnw[8] = 2
t = caput('KOYO1:CnOutWArray', cnw)
wait, 1

cnw[8]=0
t = caput('KOYO1:CnOutWArray', cnw)
wait, 1

end

