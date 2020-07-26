import epics

def testDataTypes(prefix, value):
  
  pvs = ('UINT16',
         'INT16SM',
         'BCD_UNSIGNED',
         'BCD_SIGNED',
         'INT16',
         'INT32_LE',
         'INT32_LE_BS',
         'INT32_BE',
         'INT32_BE_BS',
         'UINT32_LE',
         'UINT32_LE_BS',
         'UINT32_BE',
         'UINT32_BE_BS',
         'INT64_LE',
         'INT64_LE_BS',
         'INT64_BE',
         'INT64_BE_BS',
         'UINT64_LE',
         'UINT64_LE_BS',
         'UINT64_BE',
         'UINT64_BE_BS',
         'FLOAT32_LE',
         'FLOAT32_LE_BS',
         'FLOAT32_BE',
         'FLOAT32_BE_BS',
         'FLOAT64_LE',
         'FLOAT64_LE_BS',
         'FLOAT64_BE',
         'FLOAT64_BE_BS')
  for pv in pvs:
    epics.caput(prefix + pv, value)
