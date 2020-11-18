import sys
import fileinput
import re

for line in fileinput.input(sys.argv[1], inplace=1):
	if re.match("main:", line) is not None:
		print(line, end="")
		print("; inserting stack protection")
		print(".LPROT:")
		print("\tMOV &0x4400, R12")
		# Assumption: R12 is not populated at this moment
		print("\tCMP.W #-1, R12 { JNE\t.LPROTEND ; check prev checkpoint") 
		# only on first boot, manually set SP to NV stack. Other times 
		# restore routine sets the SP
		print("\tMOV #20540, R1 ; NV stack protection")
		print(".LPROTEND:")
	else:
		print(line, end="")
