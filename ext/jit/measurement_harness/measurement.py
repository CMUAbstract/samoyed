import serial

with serial.Serial('/dev/ttyACM1', 115200) as ser:
    while True:
        line = str(ser.readline())
        if "CNT" in line:
            overflow = int(line.split(' ')[1])
            count = int(line.split(' ')[2].split('\\r')[0])
            if overflow == 1:
                fraction = 0 # very small!
            else:
                fraction = 1 / count

            print(fraction)

