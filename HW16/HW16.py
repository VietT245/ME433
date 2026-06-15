import serial
import time
import matplotlib.pyplot as plt

PORT = "/dev/tty.usbmodem2102"
BAUD = 115200

ser = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(2)

# send start command
ser.write(b'a')

adc_data = []
current_data = []

print("Collecting data...")

start_time = time.time()

while True:
    line = ser.readline().decode(errors="ignore").strip()

    if not line:
        continue

    print(line)

    # expected format: ADC=1234, I=56
    if "ADC=" in line and "I=" in line:
        try:
            parts = line.replace("ADC=", "").replace("I=", "").split(",")
            adc = int(parts[0])
            current = int(parts[1])

            adc_data.append(adc)
            current_data.append(current)

        except:
            pass

    # stop condition (adjust as needed)
    if len(current_data) > 300:
        break

ser.close()

# plot
plt.figure()
plt.plot(current_data)
plt.title("Current Response")
plt.xlabel("Sample")
plt.ylabel("Current (mA)")
plt.grid()

plt.figure()
plt.plot(adc_data)
plt.title("ADC Position")
plt.xlabel("Sample")
plt.ylabel("ADC")
plt.grid()

plt.show()