#!/usr/bin/env python3
import sys
import hashlib
import re

property_type = [
    "MD5",
    "Cartridge_Manufacturer",
    "Cartridge_ModelNo",
    "Cartridge_Name",
    "Cartridge_Note",
    "Cartridge_Rarity",
    "Cartridge_Sound",
    "mapper", # Cartridge_Type
    "difficulty", # Console_LeftDifficulty
    "Console_RightDifficulty",
    "Console_TelevisionType",
    "control_swap", # Console_SwapPorts
    "control", # Controller_Left
    "Controller_Right",
    "paddle_swap", # Controller_SwapPaddles
    "mouse_axis", # Controller_MouseAxis
    "region", # Display_Format
    "yoffset", # Display_YStart
    "height", # Display_Height
#    "Display_Phosphor",
#    "Display_PPBlend",
]

#Cartridge_Type values: , 2IN1, CM, 4IN1, 32IN1, FE, F6SC, 4K, 8IN1, 2K, 16IN1
#Console_SwapPorts values: , YES
#Controller_Left values: , KEYBOARD, PADDLES, GENESIS, DRIVING, BOOSTERGRIP, AMIGAMOUSE, PADDLES_IAXDR, COMPUMATE, PADDLES_IAXIS, TRACKBALL22, TRACKBALL80, MINDLINK
#Controller_SwapPaddles values: , YES
#Controller_MouseAxis values: , 02, 01, 45, 78, 10, 58
#Display_YStart values: , 37, 64, 20, 56, 14, 57, 22, 43, 46, 42, 50, 59, 31, 33, 49, 36, 28, 63, 15, 62, 21, 23, 18, 25, 27, 54, 60, 39, 44, 24, 10, 38, 32, 29, 30, 8, 52, 48, 40, 26, 0
#Display_Height values: , 214, 240, 222, 223, 260, 245, 217, 230, 254, 220, 225, 215, 216, 218, 235, 256, 280


def find_entry(md5, data):
    for entry in data:
        if md5 in entry:
            return entry
    return None

def create_entry_dict(property_type, entry):
    return {prop_type: value for prop_type, value in zip(property_type, entry)}

def main():
    n = len(sys.argv)

    if n < 3: print("Usage :\a2600rominfo.py property file.a26\n"); sys.exit(0)

    romFileName = sys.argv[2]
    property_name = sys.argv[1]
    romFile = open(romFileName, 'rb')
    md5 = hashlib.md5(romFile.read()).hexdigest()
    romFile.close()

    databasePath = './stella2014-go/stella/src/emucore/DefProps.hxx'
    with open(databasePath, 'r') as file:
        data = [re.findall('"([^"]*)"', line) for line in file]

    entry = find_entry(md5, data)
    if entry:
        entry_dict = create_entry_dict(property_type, entry)
        property_value = entry_dict.get(property_name, None)
        if property_value is not None:
            print(f"{property_value}")
        else:
            print("")
    else:
        print("")


    sys.exit(0)

if __name__ == "__main__":
    main()


