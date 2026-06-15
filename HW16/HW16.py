import serial
import time
import matplotlib.pyplot as plt

PORT = "/dev/tty.usbmodem2102"
BAUD = 115200

ser = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(2)

desired_data = []
adc_data = []
current_data = []

print("Collecting data...")

start_time = time.time()

while True:
    line = ser.readline().decode(errors="ignore").strip()

    if not line:
        continue

    print(line)

    if line == "Done":
        break

    if "D=" in line and "I=" in line and "ADC=" in line:
        try:
            parts = line.split(",")

            desired = int(parts[0].replace("D=", "").strip())
            current = int(parts[1].replace("I=", "").strip())
            adc = int(parts[2].replace("ADC=", "").strip())

            desired_data.append(desired)
            current_data.append(current)
            adc_data.append(adc)

        except Exception as e:
            print("Parse error:", e)

    # stop condition (adjust as needed)
    if len(current_data) >= 400:
        break

ser.close()

# plot
plt.figure()
plt.plot(desired_data, label="Desired Current")
plt.plot(current_data, label="Actual Current")

plt.title("Current Controller Response")
plt.xlabel("Sample")
plt.ylabel("Current (mA)")
plt.legend()
plt.grid()

plt.figure()
plt.plot(adc_data)
plt.title("ADC Position")
plt.xlabel("Sample")
plt.ylabel("ADC")
plt.grid()

plt.show()