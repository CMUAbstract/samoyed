import sys

SIZE = int(sys.argv[1])
print(SIZE)

srcFile = open('src.txt', 'w')
dstFile = open('dst.txt', 'w')

for i in range(SIZE):
	srcFile.write(str(i))
	dstFile.write("77")
	if i != SIZE - 1:
		srcFile.write(",")
		dstFile.write(",")

srcFile.close()
dstFile.close()
