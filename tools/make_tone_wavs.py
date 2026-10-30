"""Make reference WAVs to test a robot's receiver using a phone/PC speaker."""
import argparse
import math
from pathlib import Path
import struct
import wave


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('output',type=Path)
    folder=parser.parse_args().output
    folder.mkdir(parents=True,exist_ok=True)
    for name,hz in [('bulbasaur',1000),('charmander',1400),('squirtle',1800)]:
        samples=[0]*8000
        for tone in (2400,hz,2800):
            for i in range(4800):
                # A short ramp avoids clicks without changing the 300 ms note.
                envelope=min(1,i/80,(4799-i)/80)
                samples.append(round(12000*envelope*math.sin(2*math.pi*tone*i/16000)))
            samples.extend([0]*1600)
        samples.extend([0]*8000)
        path=folder/f'{name}-tonos.wav'
        with wave.open(str(path),'wb') as wav:
            wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(16000)
            wav.writeframes(struct.pack('<'+'h'*len(samples),*samples))
        print(path)


if __name__=='__main__':main()
