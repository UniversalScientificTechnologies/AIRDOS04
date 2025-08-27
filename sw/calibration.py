import hid
import time
import matplotlib.pyplot as plt
import pandas as pd
from matplotlib.pyplot import cm
import numpy as np
import matplotlib

VID = 0x0403
PID = 0x6030
INTERFACE = 1

# Tady se nalezne spravny HID zarizeni dle VID/PID a vybere se UART interface

time.sleep(1)

devices = hid.enumerate()

for d in devices:
    if d['vendor_id'] == VID and d['product_id'] == PID:
        print("Nalezeno zařízení:", d)

        if d['interface_number'] == INTERFACE:
            try:
                device = hid.device()
                device.open_path(d['path'])
            finally:
                print("Vybrane zarizeni", device)
                #device.close()
                
# Vypnuti LEDEk (resp. prepnuti do RX/TX modu)
device.send_feature_report([0xA1, 0x08, 0x04])  # Nastavit GPIOA do TX_LED
device.send_feature_report([0xA1, 0x09, 0x05])  # Nastavit GPIOG do RX_LEDdevice.send_feature_report([0xB0, 0x00, 0x00, 0b00100000, 0b00100000])
# Nastaveni RTS/CTS
device.send_feature_report([0xA1, 0x03, 0x01])
br = 115200
brb = (br).to_bytes(4, byteorder="little")
payload = [0xA1, 0x42] + list(brb)
device.send_feature_report(payload)
text = ''
plt.figure(figsize=(25,10), facecolor='lightyellow')
matplotlib.rcParams.update({'font.size': 18})

n = 0

spectrum = pd.DataFrame(0, index=np.arange(1024), columns=['sum']) 

while True:
    data = device.read(64)
    if not data:
        continue

    # Zpracování dat
    text = text + bytes(data[2:2+data[1]]).decode('utf-8')
    if (text[-1] == '\n'):
        #print(str(text))

        wave = pd.DataFrame()
        wave['field'] = [int(item) if item.isdigit() else 0 for item in text.split(',')[4:]]

        #print(str(text).split(',')[4:-1])
        #print(wave)

        
        if (n>0):
            spectrum['sum'] = spectrum['sum'] + wave['field']
            #print(wave)

            plt.plot(spectrum.index, spectrum['sum'], lw=1, drawstyle='steps-post')
            #print(spectrum)
            plt.yscale('symlog')
            #plt.xscale('symlog')
            
            #plt.xlim(830,950)
            
            plt.xlabel('Ch.')
            plt.ylabel('Counts')

            # DRAW, PAUSE AND CLEAR
            plt.draw()
            plt.pause(8)
            plt.clf()

        text = ''
        n += 1
    
