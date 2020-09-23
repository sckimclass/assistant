AD82011Reg = [
                     [0x00, 0x00], #State_Control_1
                     [0x01, 0x81], #State_Control_2
                     [0x02, 0x50], #State_Control_3
                     [0x03, 0x4e], #Master_volume_control
                     [0x04, 0x18], #Channel_1_volume_control
                     [0x05, 0x18], #Channel_2_volume_control
                     [0x06, 0xa2], #Under_Voltage_selection_for_high_voltage_supply
                     [0x07, 0xfe], #State_control_4
                     [0x08, 0x6a], #DRC_limiter_attack/release_rate
                     [0x09, 0x60], #Prohibited
                     [0x0c, 0x32], #Prohibited
                     [0x0d, 0x00], #Prohibited
                     [0x0e, 0x00], #Prohibited
                     [0x0f, 0x00], #Prohibited
                     [0x10, 0x20], #Top_5_bits_of_Attack_Threshold
                     [0x11, 0x00], #Middle_8_bits_of_Attack_Threshold
                     [0x12, 0x00], #Bottom_8_bits_of_Attack_Threshold
                     [0x13, 0x20], #Top_8_bits_of_Power_Clipping
                     [0x14, 0x00], #Middle_8_bits_of_Power_Clipping
                     [0x15, 0x00], #Bottom_8_bits_of_Power_Clipping
                     [0x16, 0x00], #State_control_5
                     [0x17, 0x00], #Volume_Fine_Tune
                     [0x18, 0x40], #DDynamic_Temperature_Control
                     [0x1a, 0x00], #Top_8_bits_of_Noise_Gate_Attack_Level
                     [0x1b, 0x00], #Middle_8_bits_of_Noise_Gate_Attack_Level
                     [0x1c, 0x1a], #Bottom_8_bits_of_Noise_Gate_Attack_Level
                     [0x1d, 0x00], #Top_8_bits_of_Noise_Gate_Release_Level
                     [0x1e, 0x00], #Middle_8_bits_of_Noise_Gate_Release_Level
                     [0x1f, 0x53], #Bottom_8_bits_of_Noise_Gate_Release_Level
                     [0x20, 0x00], #Top_8_bits_of_DRC_Energy_Coefficient
                     [0x21, 0x10], #Bottom_8_bits_of_DRC_Energy_Coefficient
                     [0x22, 0x08], #Top_8_bits_of_Release_Threshold_For_DRC
                     [0x23, 0x00], #Middle_8_bits_of_Release_Threshold
                     [0x24, 0x00], #Bottom_8_bits_of_Release_Threshold
                     [0x25, 0x34], #Device Number
                     [0x2e, 0x30], #
                     [0x2f, 0x06], #
                     ]

import smbus 
bus = smbus.SMBus(1) 

AD82011_I2C = 0x34

for i in range(len(AD82011Reg)):
	bus.write_byte_data(AD82011_I2C, AD82011Reg[i][0], AD82011Reg[i][1])
