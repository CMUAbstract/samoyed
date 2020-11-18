import sys

SIZE = int(sys.argv[1])
print(SIZE)

txtFile = open('text.txt', 'w')

for i in range(SIZE):
	if i == SIZE - 1:
		txtFile.write("'\\0'")
	else:
		if i + ord('A') > ord('Z'):
			if i - (ord('Z') - ord('A') + 1) + ord('a') > ord('z'):
				assert(False)
			else:
				c = i - (ord('Z') - ord('A') + 1) + ord('a')
		else:
			c = i + ord('A')
		if i != SIZE - 1:
			txtFile.write("\'" + str(unichr(c)) + "\',")

txtFile.close()
